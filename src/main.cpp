// Voxel Engine — SDL2 + OpenGL entry point.
// FPS camera: WASD/arrows to move, mouse/finger to look, LMB/tap to mine.

#include "platform.h"
#include "renderer.h"
#include "chunk_manager.h"
#include "ore_gen.h"
#include "structure_gen.h"
#include "block_material.h"
#include "texture_atlas.h"
#include "frustum.h"
#include "sky.h"
#include "math_utils.h"
#include "time_manager.h"
#include "weather_manager.h"
#include "weather_config.h"
#include "rain_sound.h"
#include "debug_overlay.h"
#include "weather_particles.h"
#include "lightning_manager.h"
#include "craft_manager.h"
#include "craft_save.h"
#include "craft_tests.h"
#include "hotbar.h"
#include "collision.h"
#include "block_outline.h"
#include "SDL.h"
#include <cmath>
#include <cstdio>

static const int   WINDOW_WIDTH = 800;
static const int   WINDOW_HEIGHT = 600;
static const float PI           = 3.14159265358979f;
static const float MOVE_SPEED   = 8.0f;    // units/second (wider world needs faster move)
static const float LOOK_SENS    = 0.005f;
static const float PITCH_LIMIT  = 89.0f * PI / 180.0f;

// ── Player ─────────────────────────────────────────────────────────────────
struct Player {
    float x = 8.0f, y = 150.0f, z = 8.0f; // y is feet position; camera uses eye height.
    float yaw   = 0.0f;
    float pitch = -0.4f;
    float vx = 0, vy = 0, vz = 0;
    bool  grounded = false;
};
static Player player;

static float playerEyeY() {
    return player.y + EYE_HEIGHT;
}

// ── Raycast state (updated every frame for block outline) ───────────────────
static RaycastResult g_lookAt;
static bool          g_hasTarget = false;

// ── Block outline renderer ─────────────────────────────────────────────────
static BlockOutline g_blockOutline;

// ── Key state ──────────────────────────────────────────────────────────────
static bool keyW = false, keyS = false, keyA = false, keyD = false;
static bool keySpace = false, keyShift = false; // vertical flight for testing
static bool showDebug = false; // F12 toggles weather debug overlay

// ── Craft system forward reference ─────────────────────────────────────────
static Craft::CraftManager* g_craftMgr = nullptr;

// ── Touch/drag state ───────────────────────────────────────────────────────
static bool dragging    = false;
static int  lastTouchX  = 0, lastTouchY  = 0;
static int  touchStartX = 0, touchStartY = 0;

// ── Apply yaw/pitch rotation ───────────────────────────────────────────────
static void applyLook(int dx, int dy) {
    player.yaw   += dx * LOOK_SENS;
    player.pitch += dy * LOOK_SENS;
    if (player.pitch >  PITCH_LIMIT) player.pitch =  PITCH_LIMIT;
    if (player.pitch < -PITCH_LIMIT) player.pitch = -PITCH_LIMIT;
}

// ── Player physics movement ────────────────────────────────────────────────
static void updateMovement(float dt, const ChunkManager& world) {
    float fwdX = sinf(player.yaw), fwdZ = cosf(player.yaw);
    float rgtX = cosf(player.yaw), rgtZ = -sinf(player.yaw);

    // Desired movement direction (XZ plane)
    float moveX = 0, moveZ = 0;
    if (keyW) { moveX += fwdX; moveZ += fwdZ; }
    if (keyS) { moveX -= fwdX; moveZ -= fwdZ; }
    if (keyA) { moveX -= rgtX; moveZ -= rgtZ; }
    if (keyD) { moveX += rgtX; moveZ += rgtZ; }

    // Normalize diagonal movement
    float len = sqrtf(moveX * moveX + moveZ * moveZ);
    if (len > 0.001f) { moveX /= len; moveZ /= len; }

    // Target velocity
    float speed = WALK_SPEED;
    float targetVX = moveX * speed;
    float targetVZ = moveZ * speed;

    // Smooth velocity towards target (ground vs air)
    float friction = player.grounded ? FRICTION_GROUND : FRICTION_AIR;
    float k = 1.0f - expf(-friction * dt);
    player.vx += (targetVX - player.vx) * k;
    player.vz += (targetVZ - player.vz) * k;

    // Jump
    if (keySpace && player.grounded) {
        player.vy = JUMP_VELOCITY;
        player.grounded = false;
    }

    // Fly mode: Shift held = no gravity, descend at walk speed
    // Space while airborne = ascend (creative fly)
    if (keyShift) {
        player.vy = -WALK_SPEED;
    } else if (keySpace && !player.grounded) {
        player.vy = WALK_SPEED;  // fly up
    }

    // Resolve collision
    PhysicsPlayer pp;
    pp.x = player.x; pp.y = player.y; pp.z = player.z;
    pp.vx = player.vx; pp.vy = player.vy; pp.vz = player.vz;
    pp.grounded = player.grounded;

    resolveCollision(pp, world, dt);

    player.x = pp.x; player.y = pp.y; player.z = pp.z;
    player.vx = pp.vx; player.vy = pp.vy; player.vz = pp.vz;
    player.grounded = pp.grounded;
}

// ── Build MVP matrix ───────────────────────────────────────────────────────
static void computeMVP(float* mvp, int screenW, int screenH) {
    float proj[16], view[16];
    float aspect = (float)screenW / (float)screenH;
    mat4_perspective(proj, 70.0f * PI / 180.0f, aspect, 0.1f, 500.0f);

    float fwdX = cosf(player.pitch) * sinf(player.yaw);
    float fwdY = sinf(player.pitch);
    float fwdZ = cosf(player.pitch) * cosf(player.yaw);

    mat4_lookAt(view,
        player.x,         playerEyeY(),        player.z,
        player.x + fwdX,  playerEyeY() + fwdY, player.z + fwdZ,
        0.0f, 1.0f, 0.0f);

    mat4_multiply(mvp, proj, view);
}

// ── Raycast block break (precise voxel traversal) ──────────────────────────
static bool doBreakBlock(ChunkManager& world) {
    if (!g_hasTarget) return false;

    uint8_t blk = world.getBlock((float)g_lookAt.blockX + 0.5f,
                                 (float)g_lookAt.blockY + 0.5f,
                                 (float)g_lookAt.blockZ + 0.5f);
    if (blk == BLOCK_AIR) return false;

    bool ok = world.setBlock((float)g_lookAt.blockX + 0.5f,
                             (float)g_lookAt.blockY + 0.5f,
                             (float)g_lookAt.blockZ + 0.5f,
                             BLOCK_AIR);
    if (ok) {
        SDL_Log("[Mine] Block removed at (%d,%d,%d)", g_lookAt.blockX, g_lookAt.blockY, g_lookAt.blockZ);
        if (g_craftMgr) g_craftMgr->giveBlock(blk, 1);
    }
    return ok;
}

// ── Place block from selected hotbar slot (precise face placement) ──────────
static bool doPlaceBlock(ChunkManager& world) {
    if (!g_craftMgr) return false;
    if (!g_hasTarget) return false;

    int sel = g_craftMgr->inventory().selectedSlot();
    const Craft::ItemSlot& s = g_craftMgr->inventory().slot(sel);
    if (s.item == Craft::ItemID::None || s.count == 0) return false;
    const Craft::ItemInfo& info = Craft::itemInfo(s.item);
    if (info.placeAs == BLOCK_AIR) return false;

    // Placement position = hit block + face normal
    float px = g_lookAt.placeX + 0.5f;
    float py = g_lookAt.placeY + 0.5f;
    float pz = g_lookAt.placeZ + 0.5f;

    // Check: don't place inside the player
    float eyeX = player.x, eyeY = playerEyeY(), eyeZ = player.z;
    if (placementOverlapsPlayer(px, py, pz, player.x, player.y, player.z)) {
        return false;
    }

    // Check target position is air
    if (world.getBlock(px, py, pz) != BLOCK_AIR) return false;

    bool ok = world.setBlock(px, py, pz, (uint8_t)info.placeAs);
    if (ok) {
        g_craftMgr->inventory().removeItem(s.item, 1);
        SDL_Log("[Build] Placed %s at (%d,%d,%d)", info.name,
                g_lookAt.blockX + g_lookAt.faceX,
                g_lookAt.blockY + g_lookAt.faceY,
                g_lookAt.blockZ + g_lookAt.faceZ);
    }
    return ok;
}

// ── Spawn selector: jump the player to the nearest column of a biome ─────────
static void teleportToBiome(ChunkManager& world, uint8_t biome) {
    float sx, sy, sz;
    if (world.findBiomeSpawn(biome, sx, sy, sz)) {
        player.x = sx; player.y = sy; player.z = sz;
        SDL_Log("[Spawn] Teleported to %s at (%.0f, %.0f, %.0f)",
                Biomes::info((Biomes::Biome)biome).name, sx, sy, sz);
    } else {
        SDL_Log("[Spawn] No %s biome found within search range",
                Biomes::info((Biomes::Biome)biome).name);
    }
}

static void teleportToCave(ChunkManager& world) {
    float sx, sy, sz;
    if (world.findCaveSpawn(player.x, player.z, sx, sy, sz)) {
        player.x = sx; player.y = sy; player.z = sz;
        SDL_Log("[Spawn] Dropped into a cave at (%.0f, %.0f, %.0f)", sx, sy, sz);
    } else {
        SDL_Log("[Spawn] No cave found in loaded chunks yet — move around and retry");
    }
}

static void teleportToVillage(ChunkManager& world) {
    if (!world.villageIsPlaced()) {
        SDL_Log("[Spawn] Starter village not placed yet");
        return;
    }
    float sx, sy, sz;
    world.getVillageCoords(sx, sy, sz);
    player.x = sx; player.y = sy; player.z = sz;
    SDL_Log("[Spawn] Returned to starter village at (%.0f, %.0f, %.0f)", sx, sy, sz);
}

// ── Event handling ─────────────────────────────────────────────────────────
// Forward declaration for weather forcing from key handlers
static WM::WeatherManager*  g_weather   = nullptr;
static WT::TimeManager*     g_worldTime = nullptr;

static void handleEvents(bool& running, int screenW, int screenH,
                         ChunkManager& world,
                         Craft::CraftManager& craftMgr) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT: running = false; break;

        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE: running = false; break;
            case SDLK_w: case SDLK_UP:         keyW     = true;  break;
            case SDLK_s: case SDLK_DOWN:       keyS     = true;  break;
            case SDLK_a: case SDLK_LEFT:       keyA     = true;  break;
            case SDLK_d: case SDLK_RIGHT:      keyD     = true;  break;
            case SDLK_SPACE:                   keySpace = true;  break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: keyShift = true; break;
            // Spawn selector — jump to the nearest column of each biome
            case SDLK_1: teleportToBiome(world, Biomes::BIOME_OCEAN);     break;
            case SDLK_2: teleportToBiome(world, Biomes::BIOME_BEACH);     break;
            case SDLK_3: teleportToBiome(world, Biomes::BIOME_PLAINS);    break;
            case SDLK_4: teleportToBiome(world, Biomes::BIOME_FOREST);    break;
            case SDLK_5: teleportToBiome(world, Biomes::BIOME_DESERT);    break;
            case SDLK_6: teleportToBiome(world, Biomes::BIOME_SNOWY);     break;
            case SDLK_7: teleportToBiome(world, Biomes::BIOME_MOUNTAINS); break;
            case SDLK_8: teleportToCave(world);                          break;
            case SDLK_9: teleportToVillage(world);                       break;
            // ── Weather debug: F1-F11 force specific weather, F12 = debug toggle ─
            case SDLK_F1:  if(g_weather) g_weather->forceWeather(WM::WeatherType::Clear,        -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F2:  if(g_weather) g_weather->forceWeather(WM::WeatherType::PartlyCloudy, -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F3:  if(g_weather) g_weather->forceWeather(WM::WeatherType::Cloudy,       -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F4:  if(g_weather) g_weather->forceWeather(WM::WeatherType::Overcast,     -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F5:  if(g_weather) g_weather->forceWeather(WM::WeatherType::LightRain,    -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F6:  if(g_weather) g_weather->forceWeather(WM::WeatherType::HeavyRain,    -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F7:  if(g_weather) g_weather->forceWeather(WM::WeatherType::Thunderstorm, -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F8:  if(g_weather) g_weather->forceWeather(WM::WeatherType::LightFog,     -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F9:  if(g_weather) g_weather->forceWeather(WM::WeatherType::DenseFog,     -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F10: if(g_weather) g_weather->forceWeather(WM::WeatherType::Snow,         -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F11: if(g_weather) g_weather->forceWeather(WM::WeatherType::Blizzard,     -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
            case SDLK_F12: showDebug = !showDebug; break;
            // ── Hotbar slot keys (0-9 mapped via num-row above WASD) ─────────
            // Scroll wheel preferred; these are fallbacks
            case SDLK_KP_1: case SDLK_KP_2: case SDLK_KP_3:
            case SDLK_KP_4: case SDLK_KP_5: case SDLK_KP_6:
            case SDLK_KP_7: case SDLK_KP_8: case SDLK_KP_9: {
                int slot = (int)(ev.key.keysym.sym - SDLK_KP_1);
                craftMgr.inventory().setSelected(slot);
                break;
            }
            }
            break;

        case SDL_KEYUP:
            switch (ev.key.keysym.sym) {
            case SDLK_w: case SDLK_UP:         keyW     = false; break;
            case SDLK_s: case SDLK_DOWN:       keyS     = false; break;
            case SDLK_a: case SDLK_LEFT:       keyA     = false; break;
            case SDLK_d: case SDLK_RIGHT:      keyD     = false; break;
            case SDLK_SPACE:                   keySpace = false; break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: keyShift = false; break;
            }
            break;

        // Touch: drag = look, tap (<10px) = mine
        case SDL_FINGERDOWN:
            dragging    = true;
            lastTouchX  = (int)(ev.tfinger.x * screenW);
            lastTouchY  = (int)(ev.tfinger.y * screenH);
            touchStartX = lastTouchX;
            touchStartY = lastTouchY;
            break;

        case SDL_FINGERMOTION:
            if (dragging) {
                int x = (int)(ev.tfinger.x * screenW);
                int y = (int)(ev.tfinger.y * screenH);
                applyLook(x - lastTouchX, y - lastTouchY);
                lastTouchX = x; lastTouchY = y;
            }
            break;

        case SDL_FINGERUP: {
            int fx = (int)(ev.tfinger.x * screenW);
            int fy = (int)(ev.tfinger.y * screenH);
            int ddx = fx - touchStartX;
            int ddy = fy - touchStartY;
            // Tap (< 15px movement) = interact; drag = look
            if (ddx*ddx + ddy*ddy < 15*15) {
                // Left half of screen = mine, right half = place
                if (fx < screenW / 2)
                    doBreakBlock(world);
                else
                    doPlaceBlock(world);
            }
            dragging = false;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT)  doBreakBlock(world);
            if (ev.button.button == SDL_BUTTON_RIGHT) doPlaceBlock(world);
            break;

        case SDL_MOUSEWHEEL: {
            int sel = craftMgr.inventory().selectedSlot();
            sel = (sel - ev.wheel.y + Craft::Inventory::HOTBAR_SIZE)
                  % Craft::Inventory::HOTBAR_SIZE;
            craftMgr.inventory().setSelected(sel);
            break;
        }

        case SDL_MOUSEMOTION:
            applyLook(ev.motion.xrel, ev.motion.yrel);
            break;

        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                pglViewport(0, 0, ev.window.data1, ev.window.data2);
            break;
        }
    }
}

// ── Main ───────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOGE("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Voxel Engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    if (!window) {
        LOGE("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit(); return 1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        LOGE("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    SDL_GL_MakeCurrent(window, glCtx);
    SDL_GL_SetSwapInterval(1);

    if (!gl_load_extensions()) {
        LOGE("Failed to load OpenGL extensions");
        SDL_GL_DeleteContext(glCtx); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    LOGI("OpenGL: %s | GLSL: %s | %s",
         pglGetString(GL_VERSION),
         pglGetString(GL_SHADING_LANGUAGE_VERSION),
         pglGetString(GL_RENDERER));

    Renderer renderer;
    renderer.init();

    Sky sky;
    sky.init();

    TextureAtlas atlas;
    atlas.init();
    renderer.setAtlas(atlas.texture(), atlas.tileSizeUV(), atlas.texelUV());

    // Textured vertex attribute locations for chunk meshing
    MeshAttribs meshAttr;
    meshAttr.pos   = renderer.locPos;
    meshAttr.uv    = renderer.locUV;
    meshAttr.tile  = renderer.locTile;
    meshAttr.light = renderer.locLight;
    meshAttr.sky   = renderer.locSky;
    meshAttr.block = renderer.locBlock;
    meshAttr.wave  = renderer.locWave;

    // Scope the world so its destructor (worker join + chunk GL buffer frees)
    // runs while the GL context is still current, before we tear the context down.
    {
    ChunkManager world(12345u);

    // ── World Time ─────────────────────────────────────────────────────────
    WT::TimeManager worldTime;
    // Always start at DEFAULT_START_SECS (10:00) — don't load/save time on disk

    // Register event handlers — other systems (NPCs, weather, farming) will
    // subscribe here in the future.
    worldTime.events().subscribe(WT::TimeEvent::OnSunrise,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] ** Sunrise **"); });
    worldTime.events().subscribe(WT::TimeEvent::OnSunset,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] ** Sunset **"); });
    worldTime.events().subscribe(WT::TimeEvent::OnNoon,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] ** Noon **"); });
    worldTime.events().subscribe(WT::TimeEvent::OnMidnight,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] ** Midnight **"); });
    worldTime.events().subscribe(WT::TimeEvent::OnNewDay,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] New day begins."); });
    worldTime.events().subscribe(WT::TimeEvent::OnNewSeason,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] Season changed!"); });
    worldTime.events().subscribe(WT::TimeEvent::OnNewYear,
        [](WT::TimeEvent){ SDL_Log("[WorldTime] New year!"); });

    // ── Dynamic Weather ────────────────────────────────────────────────────
    WM::WeatherManager weather;
    weather.load("world_weather.dat");  // first run: starts with Clear
    g_weather = &weather;  // expose to key handlers
    g_worldTime = &worldTime;  // expose game time to key handlers

    // Lightning + thunder system
    WM::LightningManager lightning;

    // Event handlers — future NPC/farming/economy systems subscribe here.
    weather.events().subscribe(WM::WeatherEvent::OnWeatherChanged,
        [](WM::WeatherEvent, const WM::WeatherState& s){
            SDL_Log("[Weather] → %s (%.0f%%)", WM::weatherName(s.type),
                    s.intensity * 100.0f);
        });
    weather.events().subscribe(WM::WeatherEvent::OnRainStart,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Rain started — agriculture: +growth; economy: normal");
        });
    weather.events().subscribe(WM::WeatherEvent::OnRainEnd,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Rain ended.");
        });
    weather.events().subscribe(WM::WeatherEvent::OnStormStart,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Storm! NPCs seek shelter. Prepare thunder events.");
        });
    weather.events().subscribe(WM::WeatherEvent::OnSnowStart,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Snow! Animals seek warmth. Agriculture paused.");
        });
    weather.events().subscribe(WM::WeatherEvent::OnDrought,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Drought! Crops wither. Economy: food prices up.");
        });
    weather.events().subscribe(WM::WeatherEvent::OnThunder,
        [](WM::WeatherEvent, const WM::WeatherState& s){
            SDL_Log("[Weather] ** THUNDER + LIGHTNING ** (intensity %.0f%%)", s.intensity * 100.0f);
        });
    weather.events().subscribe(WM::WeatherEvent::OnClearSky,
        [](WM::WeatherEvent, const WM::WeatherState& s){
            SDL_Log("[Weather] Clear skies return. Temperature: %.1f°C", s.temperature);
        });
    weather.events().subscribe(WM::WeatherEvent::OnFogRoll,
        [](WM::WeatherEvent, const WM::WeatherState&){
            SDL_Log("[Weather] Fog rolling in. Visibility reduced.");
        });

    // ── Debug Overlay (on-screen weather buttons + state display) ────────────
    DebugOverlay debugOverlay;
    debugOverlay.init();

    // ── Weather Particles (rain / snow visuals) ─────────────────────────────
    WeatherParticles weatherParticles;
    weatherParticles.init(WM::weatherConfig().maxParticles);

    // ── Rain Sound (prepared interface — no audio yet) ───────────────────────
    WM::RainSound rainSound;

    // ── Block selection outline ──────────────────────────────────────────────
    g_blockOutline.init();

    // ── Crafting system + hotbar ─────────────────────────────────────────────
    Craft::CraftManager craftMgr;
    UI::Hotbar          hotbar;
    Craft::loadCraft(craftMgr, "world_craft.dat");
    g_craftMgr = &craftMgr;
    if (!Craft::runCraftTests())
        SDL_Log("[Craft] WARNING: some craft tests FAILED");
    else
        SDL_Log("[Craft] All tests passed.");

    SDL_SetRelativeMouseMode(SDL_TRUE);
    LOGI("Controls: WASD=move, Space/Shift=fly up/down, mouse=look, LMB=mine, ESC=exit");
    LOGI("Spawn selector: 1=Ocean 2=Beach 3=Plains 4=Forest 5=Desert 6=Snowy 7=Mountains 8=Cave 9=Village");

    // Spawn at a safe village position. player.y is feet height; camera is offset by EYE_HEIGHT.
    if (world.villageIsPlaced()) {
        float vx, vy, vz;
        world.getVillageCoords(vx, vy, vz);
        player.x = vx; player.y = vy; player.z = vz;
        player.vx = player.vy = player.vz = 0.0f;
        player.grounded = false;
        LOGI("[Village] Starter village at (%d, %d) - spawning at safe surface y=%.1f",
             world.villageOriginX(), world.villageOriginZ(), player.y);
    }

    // Perf/test: VOXEL_GOTO=<biome index> spawns straight in that biome.
    if (const char* g = SDL_getenv("VOXEL_GOTO"))
        teleportToBiome(world, (uint8_t)SDL_atoi(g));

    Uint32 startTicks = SDL_GetTicks();
    Uint32 lastTime   = startTicks;
    bool   running    = true;
    int    frameNum   = 0;
    float  fps        = 0.0f;
    float  worstDt    = 0.0f;   // worst frame time since last report (hitch probe)
    float  renderMs   = 0.0f;   // CPU time to submit the world draw calls
    // Ambient colour smoothly follows the biome under the player
    float  ambient[3] = { 0.70f, 0.82f, 0.92f };

    while (running) {
        Uint32 now = SDL_GetTicks();
        float  dt  = (now - lastTime) * 0.001f;
        lastTime   = now;
        float  time = (now - startTicks) * 0.001f;   // seconds since start
        frameNum++;
        if (dt > 0.0f) fps = fps * 0.9f + (1.0f / dt) * 0.1f;   // smoothed FPS
        if (dt > worstDt) worstDt = dt;                         // track hitches

        int screenW, screenH;
        SDL_GetWindowSize(window, &screenW, &screenH);

        handleEvents(running, screenW, screenH, world, craftMgr);
        updateMovement(dt, world);

        // Debug: log player position for first 120 frames (2 seconds)
        if (frameNum <= 120 && frameNum % 10 == 0) {
            uint8_t belowBlock = world.getBlock(player.x, player.y - 0.1f, player.z);
            uint8_t feetBlock  = world.getBlock(player.x, player.y, player.z);
            SDL_Log("[Player] pos=(%.1f,%.1f,%.1f) vy=%.1f grounded=%d below=%d feet=%d chunks=%d",
                    player.x, player.y, player.z, player.vy, player.grounded,
                    belowBlock, feetBlock, world.loadedChunkCount());
        }

        // ── Update block selection raycast ──────────────────────────────────
        {
            float dx = cosf(player.pitch) * sinf(player.yaw);
            float dy = sinf(player.pitch);
            float dz = cosf(player.pitch) * cosf(player.yaw);
            float eyeX = player.x, eyeY = playerEyeY(), eyeZ = player.z;
            g_hasTarget = raycastVoxel(world, eyeX, eyeY, eyeZ,
                                       dx, dy, dz, MAX_REACH, g_lookAt);
        }

        // Perf/debug: set VOXEL_AUTOWALK to stream chunks continuously (proves
        // background generation causes no hitch while the world loads/unloads).
        static bool autoWalk = SDL_getenv("VOXEL_AUTOWALK") != nullptr;
        if (autoWalk) player.x += MOVE_SPEED * dt;

        // Update world time (every frame — sun angle is continuous)
        worldTime.update(dt);

        // Update weather (game-time-driven transitions, smooth lerp, event firing)
        {
            const WT::CalendarState& cal = worldTime.calendar();
            float hourOfDay = cal.hour + cal.minute / 60.0f;
            weather.update(dt, worldTime.totalGameSecs(), cal,
                           world.biomeAtWorld(player.x, player.z),
                           hourOfDay, player.y, 64.0f);
        }

        // Update rain/snow particles
        {
            const float camFwd[3] = { cosf(player.pitch) * sinf(player.yaw),
                                      sinf(player.pitch),
                                      cosf(player.pitch) * cosf(player.yaw) };
            const float camPos[3] = { player.x, playerEyeY(), player.z };
            weatherParticles.update(dt, weather.rainRate(), weather.snowRate(),
                                    weather.state().windSpeed, camPos, camFwd);
        }

        // Update rain sound parameters
        rainSound.update(weather.state(), WM::weatherConfig());

        // Update lightning + thunder (handles flash, delay, randomised intervals)
        lightning.update(dt, weather.hasThunder(), weather.state().intensity);

        // Fire weather event when thunder audio should play
        if (lightning.thunderJustFired()) {
            weather.events().fire(WM::WeatherEvent::OnThunder, weather.state());
        }

        // Update crafting timers and furnace
        craftMgr.update(dt);

        // Update world (load/unload chunks, rebuild dirty meshes)
        world.update(player.x, player.z, meshAttr);

        float mvp[16];
        computeMVP(mvp, screenW, screenH);
        Frustum frustum;
        frustum.fromMatrix(mvp);

        // Camera basis (for the sky ray reconstruction)
        float fwd[3] = { cosf(player.pitch) * sinf(player.yaw),
                         sinf(player.pitch),
                         cosf(player.pitch) * cosf(player.yaw) };
        float wup[3] = { 0.0f, 1.0f, 0.0f };
        float right[3] = { fwd[1]*wup[2] - fwd[2]*wup[1],
                           fwd[2]*wup[0] - fwd[0]*wup[2],
                           fwd[0]*wup[1] - fwd[1]*wup[0] };
        float rl = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        if (rl > 0) { right[0]/=rl; right[1]/=rl; right[2]/=rl; }
        float up[3] = { right[1]*fwd[2] - right[2]*fwd[1],
                        right[2]*fwd[0] - right[0]*fwd[2],
                        right[0]*fwd[1] - right[1]*fwd[0] };

        float aspect = (float)screenW / (float)screenH;
        float tanY = tanf(0.5f * 70.0f * PI / 180.0f);
        float tanX = tanY * aspect;

        // Sun direction from WorldTime (replaces raw time * 0.02f hack)
        float sun[3];
        worldTime.sunDir(sun);

        // Environment colour follows the biome under the player (smoothed)
        Biomes::Biome curBiome = (Biomes::Biome)world.biomeAtWorld(player.x, player.z);
        const float* target = Biomes::info(curBiome).ambient;
        float k = 1.0f - expf(-dt * 2.0f);        // frame-rate independent lerp
        for (int i = 0; i < 3; i++)
            ambient[i] += (target[i] - ambient[i]) * k;

        const float camPos[3] = { player.x, playerEyeY(), player.z };
        // Fog distances driven by FogManager (biome × time-of-day × weather)
        float fogStart = weather.fog().fogStart();
        float fogEnd   = weather.fog().fogEnd();

        // ── Report every 5 seconds ─────────────────────────────────────────
        if (frameNum % 300 == 0) {
            worldTime.debugLog();
            int hist[Biomes::BIOME_COUNT];
            world.biomeHistogram(hist);
            char biomes[256]; int off = 0;
            for (int i = 0; i < Biomes::BIOME_COUNT; i++)
                if (hist[i] > 0)
                    off += snprintf(biomes + off, sizeof(biomes) - off, "%s:%d ",
                                    Biomes::info((Biomes::Biome)i).name, hist[i]);
            SDL_Log("[Report] FPS=%.1f | chunks=%d (drawn=%d culled=%d, pending=%d) | verts=%ld tris=%d | blocks=%ld",
                    fps, world.loadedChunkCount(), world.drawnChunkCount(),
                    world.culledChunkCount(), world.pendingChunks(), world.totalVertices(),
                    world.drawnIndexCount() / 3, world.totalBlocks());
            SDL_Log("[Report] biome@player=%s | biomes: %s",
                    Biomes::info(curBiome).name, biomes);

            long ores[6]; world.oreHistogram(ores);
            SDL_Log("[Underground] maxHeight=%d | caveBlocksRemoved=%ld | genTime=%.2fms/chunk",
                    world.maxTerrainHeight(), world.totalCaveRemoved(),
                    world.lastGenMillis());
            char oreStr[256]; int oo = 0;
            for (int i = 0; i < Ores::ORE_COUNT; i++)
                oo += snprintf(oreStr + oo, sizeof(oreStr) - oo, "%s:%ld ",
                               Ores::ORES[i].name, ores[i]);
            SDL_Log("[Underground] ores: %s", oreStr);
            { int litB = 0, darkB = 0;
              world.lightStats(litB, darkB);
              SDL_Log("[Lighting] totalLightMs=%.2f | litBlocks=%d darkBlocks=%d | cross-chunk BFS active",
                      world.totalLightMs(), litB, darkB); }
            SDL_Log("[Perf] worstFrame=%.1fms (last 5s) | bg-gen thread active",
                    worstDt * 1000.0f);
            worstDt = 0.0f;   // reset for next window
            SDL_Log("[Textures] atlas=%dx%d tiles=%d | materials=%d | drawCalls=%d | renderSubmit=%.2fms",
                    Atlas::ATLAS_PX, Atlas::ATLAS_PX, atlas.tileCount(),
                    Materials::materialCount(), world.drawnChunkCount() + 1, renderMs);
            SDL_Log("[Structures] loaded=%d | registry=%d types | genTime=%.2fms/chunk",
                    world.totalStructures(), Structures::count(), world.lastGenMillis());
            if (world.villageIsPlaced())
                SDL_Log("[Village] origin=(%d,%d) blocks=%d overrides=%d | press 9 to return",
                        world.villageOriginX(), world.villageOriginZ(),
                        world.villageBlocks(), world.saveOverrideCount());

            // Weather report
            weather.debugLog();

            // One-shot: confirm caves exist in the loaded world (press 8 to visit)
            static bool caveChecked = false;
            if (!caveChecked) {
                caveChecked = true;
                float cx, cy, cz;
                if (world.findCaveSpawn(player.x, player.z, cx, cy, cz)) {
                    SDL_Log("[Cave] nearest cave entrance at (%.0f,%.0f,%.0f) — press 8 to drop in",
                            cx, cy, cz);
                    if (SDL_getenv("VOXEL_CAVE")) teleportToCave(world);  // perf/visual test
                } else {
                    SDL_Log("[Cave] no cave in loaded chunks near spawn");
                }
            }
        }

        pglViewport(0, 0, screenW, screenH);
        renderer.beginFrame();
        {
            const WM::WeatherState& ws = weather.state();
            float cloudRGB[3] = { ws.cloudR, ws.cloudG, ws.cloudB };
            sky.render(right, up, fwd, tanX, tanY, sun, time, ambient,
                       weather.cloudCover(), weather.lightMult(),
                       weather.wind().dx(), weather.wind().dz(),
                       cloudRGB, weather.fog().density());
        }
        // Sun brightness from WorldTime: 0.35 (night/caves) → 1.0 (noon).
        // Weather dims it further during storms.
        // Lightning flash temporarily boosts brightness during thunderstorms.
        float sunLight = worldTime.sunLight() * weather.lightMult()
                       * lightning.flashBrightness();
        // 0.22 ambient ensures village and caves are never pitch-black
        renderer.setLight(sunLight, 0.22f);
        renderer.setFrame(mvp, time, camPos, ambient, fogStart, fogEnd);
        Uint64 rt0 = SDL_GetPerformanceCounter();
        world.render(frustum);
        Uint64 rt1 = SDL_GetPerformanceCounter();
        renderMs = (float)((double)(rt1 - rt0) * 1000.0 / (double)SDL_GetPerformanceFrequency());

        // Draw rain/snow particles (after world, before UI)
        weatherParticles.draw(mvp, time);

        // Draw block selection outline (after world, depth-tested)
        if (g_hasTarget) {
            g_blockOutline.draw((float)g_lookAt.blockX, (float)g_lookAt.blockY,
                                (float)g_lookAt.blockZ, mvp);
        }

        // ── 2D Overlays: hotbar (always) + debug panel (F12) ────────────────
        debugOverlay.beginFrame(screenW, screenH);

        hotbar.draw(debugOverlay, craftMgr.inventory(), screenW, screenH);

        if (showDebug) {
            const WM::WeatherState& ws  = weather.state();
            const WT::CalendarState& cs = worldTime.calendar();
            int px = 10, py = 10;

            // Background panel
            debugOverlay.drawRect(px - 4, py - 4, 390, 360, 0x0d0d1e, 0.90f);

            // Title
            debugOverlay.drawText("DEBUG (F12=hide)", px, py, 0x00ff88);
            py += 16;

            // ── World Time ────────────────────────────────────────────────────
            {
                char buf[160];
                snprintf(buf, sizeof(buf), "Time: %02d:%02d  Day %d  %s Yr%d",
                         cs.hour, cs.minute, cs.dayOfMonth,
                         WT::seasonName(cs.season), cs.year);
                debugOverlay.drawText(buf, px, py, 0xffdd88);
                py += 13;

                snprintf(buf, sizeof(buf), "Period: %s  Week %u  Month: %s",
                         WT::periodName(cs.period), (unsigned)cs.weekNum,
                         WT::monthName(cs.month));
                debugOverlay.drawText(buf, px, py, 0xcc9944);
                py += 15;
            }

            // ── Weather ───────────────────────────────────────────────────────
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Weather: %s (%.0f%%)  Trans: %.0f%%",
                         WM::weatherName(ws.type), ws.intensity * 100.0f,
                         ws.transition * 100.0f);
                debugOverlay.drawText(buf, px, py, 0xffffff);
                py += 13;

                if (ws.rainRate > 0.01f) {
                    const char* rl = WM::rainIntensityName(ws.rainRate, WM::weatherConfig());
                    snprintf(buf, sizeof(buf), "Rain: %s (%.0f%%)  Snow: %.0f%%",
                             rl, ws.rainRate * 100.0f, ws.snowRate * 100.0f);
                    debugOverlay.drawText(buf, px, py, 0x66aaff);
                } else {
                    debugOverlay.drawText("Rain: None", px, py, 0x555566);
                }
                py += 13;

                snprintf(buf, sizeof(buf), "Temp: %.1fC  Hum: %.0f%%  Vis: %.0fm",
                         ws.temperature, ws.humidity, ws.visibility);
                debugOverlay.drawText(buf, px, py, 0xaaaaaa);
                py += 13;

                snprintf(buf, sizeof(buf), "Cloud: %.0f%%  Light: %.2f  CloudRGB: %.2f,%.2f,%.2f",
                         ws.cloudCover * 100.0f, ws.lightMult,
                         ws.cloudR, ws.cloudG, ws.cloudB);
                debugOverlay.drawText(buf, px, py, 0xaaaaaa);
                py += 13;

                // Wind sub-manager
                snprintf(buf, sizeof(buf), "Wind: spd=%.2f dir=%.0fdeg  (dx=%.2f dz=%.2f)",
                         weather.wind().speed(),
                         weather.wind().direction() * 57.296f,
                         weather.wind().dx(), weather.wind().dz());
                debugOverlay.drawText(buf, px, py, 0x88ddff);
                py += 13;

                // Fog sub-manager
                snprintf(buf, sizeof(buf), "Fog: dens=%.0f%%  start=%.0f  end=%.0f  haze=%.2f",
                         weather.fog().density() * 100.0f,
                         weather.fog().fogStart(), weather.fog().fogEnd(),
                         weather.fog().horizonBlur());
                debugOverlay.drawText(buf, px, py, 0xaaddcc);
                py += 13;

                // Snow sub-manager
                static const char* PHASE_NAME[] = {"no","Building","Peak","Weakening"};
                int phIdx = (int)weather.storm().phase();
                snprintf(buf, sizeof(buf), "Snow: accum=%.0f%%  ice=%s  storm=%s",
                         weather.snow().snowLevel() * 100.0f,
                         weather.snow().hasIce() ? "YES" : "no",
                         PHASE_NAME[phIdx < 4 ? phIdx : 0]);
                debugOverlay.drawText(buf, px, py, 0xccccff);
                py += 13;

                bool stormActive = (ws.type == WM::WeatherType::Thunderstorm ||
                                    ws.type == WM::WeatherType::Blizzard);
                snprintf(buf, sizeof(buf), "Storm: %s  Thunder: %s  Lightning: %s",
                         stormActive ? "YES" : "no",
                         ws.hasThunder ? "YES" : "no",
                         lightning.isFlashing() ? "FLASH!" : "no");
                debugOverlay.drawText(buf, px, py,
                    lightning.isFlashing() ? 0xffff44 :
                    ws.hasThunder ? 0xff6644 : 0x666666);
                py += 15;
            }

            // ── Particles ─────────────────────────────────────────────────────
            {
                char buf[128];
                debugOverlay.drawText("PARTICLES:", px, py, 0xffcc00);
                py += 13;

                snprintf(buf, sizeof(buf), "Drops: %d / %d  Splashes: %d",
                         weatherParticles.activeCount(), WM::weatherConfig().maxParticles,
                         weatherParticles.splashCount());
                debugOverlay.drawText(buf, px, py, 0x88ccff);
                py += 15;
            }

            // ── World / Position ──────────────────────────────────────────────
            {
                char buf[128];
                int chunkX = (int)floorf(player.x / 16.0f);
                int chunkZ = (int)floorf(player.z / 16.0f);
                snprintf(buf, sizeof(buf), "Pos: %.0f, %.0f, %.0f  Chunk: %d,%d",
                         player.x, player.y, player.z, chunkX, chunkZ);
                debugOverlay.drawText(buf, px, py, 0x888888);
                py += 13;

                snprintf(buf, sizeof(buf), "Biome: %s  Chunks: %d (drawn=%d)",
                         Biomes::info(curBiome).name,
                         world.loadedChunkCount(), world.drawnChunkCount());
                debugOverlay.drawText(buf, px, py, 0x888888);
                py += 13;

                // Approximate memory: chunk data ~(16*128*16*1 + 2*16*128*16) = ~196KB each
                int approxMB = (world.loadedChunkCount() * 200) / 1024;
                snprintf(buf, sizeof(buf), "~Mem: %dMB chunks | Particles: %dKB",
                         approxMB,
                         (int)(WM::weatherConfig().maxParticles * sizeof(float) * 6) / 1024);
                debugOverlay.drawText(buf, px, py, 0x666677);
                py += 15;
            }

            // ── FPS ───────────────────────────────────────────────────────────
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "FPS: %.1f  Worst: %.1fms  SunLight: %.2f",
                         fps, worstDt * 1000.0f,
                         worldTime.sunLight() * weather.lightMult());
                debugOverlay.drawText(buf, px, py,
                    fps > 45 ? 0x44ff44 : fps > 25 ? 0xffaa00 : 0xff4444);
                py += 16;
            }

        } // end if (showDebug)

        debugOverlay.flush();

        SDL_GL_SwapWindow(window);
    }
    Craft::saveCraft(craftMgr, "world_craft.dat");
    // worldTime.saveToLoadPath() — not saving: always start fresh at 10:00
    weather.saveToLoadPath();
    weatherParticles.cleanup();
    g_blockOutline.cleanup();
    debugOverlay.cleanup();
    }  // world destroyed here — worker joined, chunk GL buffers freed with a live context

    atlas.cleanup();
    sky.cleanup();
    renderer.cleanup();
    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
