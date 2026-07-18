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
#include "season_manager.h"
#include "weather_config.h"
#include "rain_sound.h"
#include "debug_overlay.h"
#include "weather_particles.h"
#include "lightning_manager.h"
#include "craft_manager.h"
#include "craft_save.h"
#include "craft_tests.h"
#include "crafting_ui.h"
#include "hotbar.h"
#include "inventory_panel.h"
#include "build_preview.h"
#include "collision.h"
#include "block_outline.h"
#include "touch_controls.h"
#include "mobile_hud.h"
#include "minimap.h"
#include "settings/graphics_settings.h"
#include "settings_panel.h"
#include "SDL.h"
#include <cmath>
#include <cstdio>

static const int   WINDOW_WIDTH = 1280;
static const int   WINDOW_HEIGHT = 720;
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
static float g_inputScaleX = 1.0f, g_inputScaleY = 1.0f;
static int inputX(int x) { return (int)lroundf((float)x * g_inputScaleX); }
static int inputY(int y) { return (int)lroundf((float)y * g_inputScaleY); }

static float playerEyeY() {
    return player.y + EYE_HEIGHT;
}

// ── Raycast state (updated every frame for block outline) ───────────────────
static RaycastResult g_lookAt;
static bool          g_hasTarget = false;

// ── Block outline renderer ─────────────────────────────────────────────────
static BlockOutline g_blockOutline;
static BuildPreview g_buildPreview;
static bool g_canPlace = false;
static int g_placeX = 0, g_placeY = 0, g_placeZ = 0;
static float g_actionPulse = 0.0f;

// ── Key state ──────────────────────────────────────────────────────────────
static bool keyW = false, keyS = false, keyA = false, keyD = false;
static bool keySpace = false, keyShift = false; // vertical flight for testing
static bool showDebug = false; // F12 toggles weather debug overlay

// ── Craft system forward reference ─────────────────────────────────────────
static Craft::CraftManager* g_craftMgr = nullptr;

// ── Touch/drag state ───────────────────────────────────────────────────────
static UI::TouchControls g_touch;   // mobile HUD (also mouse-driven on PC)
static UI::MobileHud     g_hud;     // player chip, compass, coords/clock
static UI::Minimap       g_minimap;
// Player vitals (survival systems still minimal: lava burns, slow regen).
static float g_hp = 20.0f, g_food = 18.0f, g_hurtCooldown = 0.0f;
static void setInventoryInputMode(bool open) {
    // Touch mode never captures the mouse: the HUD needs an absolute cursor.
    const bool capture = !open && !g_touch.enabled();
    SDL_SetRelativeMouseMode(capture ? SDL_TRUE : SDL_FALSE);
    SDL_ShowCursor(capture ? SDL_DISABLE : SDL_ENABLE);
}
static bool dragging    = false;
static int  lastTouchX  = 0, lastTouchY  = 0;
static int  touchStartX = 0, touchStartY = 0;

// ── Apply yaw/pitch rotation ───────────────────────────────────────────────
static void applyLook(int dx, int dy) {
    // Camera drag follows the finger: right drag turns right.
    player.yaw   -= dx * LOOK_SENS;
    // Mobile drag: finger up looks up, finger down looks down.
    player.pitch -= dy * LOOK_SENS;
    if (player.pitch >  PITCH_LIMIT) player.pitch =  PITCH_LIMIT;
    if (player.pitch < -PITCH_LIMIT) player.pitch = -PITCH_LIMIT;
}

// ── Player physics movement ────────────────────────────────────────────────
static void updateMovement(float dt, const ChunkManager& world) {
    // Perf/test: VOXEL_EYE freezes the player as a static aerial camera
    // (screenshot harness) — no gravity, no drift.
    static const bool frozenCam = SDL_getenv("VOXEL_EYE") != nullptr;
    if (frozenCam) { player.vx = player.vy = player.vz = 0.0f; return; }

    float fwdX = sinf(player.yaw), fwdZ = cosf(player.yaw);
    // Match the camera view basis: screen-right is -X at yaw zero.
    float rgtX = -cosf(player.yaw), rgtZ = sinf(player.yaw);

    // Desired movement direction (XZ plane)
    float moveX = 0, moveZ = 0;
    if (keyW) { moveX += fwdX; moveZ += fwdZ; }
    if (keyS) { moveX -= fwdX; moveZ -= fwdZ; }
    if (keyA) { moveX -= rgtX; moveZ -= rgtZ; }
    if (keyD) { moveX += rgtX; moveZ += rgtZ; }
    // Virtual joystick adds its analogue vector (0 when idle/disabled).
    moveX += fwdX * g_touch.axisY() + rgtX * g_touch.axisX();
    moveZ += fwdZ * g_touch.axisY() + rgtZ * g_touch.axisX();

    // Clamp: keys give unit speed, the joystick keeps analogue gradation.
    float len = sqrtf(moveX * moveX + moveZ * moveZ);
    if (len > 1.0f) { moveX /= len; moveZ /= len; }

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
    const bool wantJump   = keySpace || g_touch.jumpHeld();
    const bool wantCrouch = keyShift || g_touch.crouchHeld();
    if (wantJump && player.grounded) {
        player.vy = JUMP_VELOCITY;
        player.grounded = false;
    }

    // Fly mode: Shift held = no gravity, descend at walk speed
    // Space while airborne = ascend (creative fly)
    if (wantCrouch) {
        player.vy = -WALK_SPEED;
    } else if (wantJump && !player.grounded) {
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
static float cameraFovY(int screenW, int screenH) {
#ifdef __ANDROID__
    // A 70 degree vertical FOV becomes an excessively wide ~120 degree
    // horizontal view on a 20:9 phone. Keep the mobile view readable.
    (void)screenW; (void)screenH;
    return 58.0f;
#else
    return ((float)screenW / (float)screenH) > 1.95f ? 60.0f : 70.0f;
#endif
}
static void computeMVP(float* mvp, int screenW, int screenH) {
    float proj[16], view[16];
    float aspect = (float)screenW / (float)screenH;
    mat4_perspective(proj, cameraFovY(screenW, screenH) * PI / 180.0f, aspect, 0.1f, 500.0f);

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
// Touch aim override: on mobile, mining/placing targets the block UNDER THE
// FINGER (Minecraft Bedrock behaviour), not the crosshair. The override ray
// is set from a screen point and cleared when no gesture is active.
static bool  g_aimOverride = false;
static float g_aimDX = 0.0f, g_aimDY = 0.0f, g_aimDZ = 1.0f;

static void aimFromScreen(int sx, int sy, int W, int H) {
    const float fovY = cameraFovY(W, H) * PI / 180.0f;
    const float tanY = tanf(fovY * 0.5f);
    const float tanX = tanY * (float)W / (float)H;
    const float nx = ((2.0f * (float)sx / (float)W) - 1.0f) * tanX;
    const float ny = (1.0f - (2.0f * (float)sy / (float)H)) * tanY;
    // Camera basis (same convention as mat4_lookAt: side = forward × up).
    const float fx = cosf(player.pitch) * sinf(player.yaw);
    const float fy = sinf(player.pitch);
    const float fz = cosf(player.pitch) * cosf(player.yaw);
    float sxv = -fz, syv = 0.0f, szv = fx;
    const float sl = sqrtf(sxv*sxv + szv*szv);
    if (sl > 1e-5f) { sxv /= sl; szv /= sl; }
    const float uxv = syv*fz - szv*fy, uyv = szv*fx - sxv*fz, uzv = sxv*fy - syv*fx;
    float dx = fx + sxv*nx + uxv*ny;
    float dy = fy + syv*nx + uyv*ny;
    float dz = fz + szv*nx + uzv*ny;
    const float dl = sqrtf(dx*dx + dy*dy + dz*dz);
    g_aimDX = dx/dl; g_aimDY = dy/dl; g_aimDZ = dz/dl;
    g_aimOverride = true;
}

static void refreshLookAt(const ChunkManager& world) {
    float dx, dy, dz;
    if (g_aimOverride) {
        dx = g_aimDX; dy = g_aimDY; dz = g_aimDZ;
    } else {
        dx = cosf(player.pitch) * sinf(player.yaw);
        dy = sinf(player.pitch);
        dz = cosf(player.pitch) * cosf(player.yaw);
    }
    g_hasTarget = raycastVoxel(world, player.x, playerEyeY(), player.z,
                               dx, dy, dz, MAX_REACH, g_lookAt);
}

static void updatePlacementPreview(const ChunkManager& world) {
    g_canPlace = false;
    if (!g_hasTarget || !g_craftMgr) return;
    int sel = g_craftMgr->inventory().selectedSlot();
    const Craft::ItemSlot& slot = g_craftMgr->inventory().slot(sel);
    const Craft::ItemInfo& info = Craft::itemInfo(slot.item);
    if (slot.item == Craft::ItemID::None || slot.count == 0 || info.placeAs == BLOCK_AIR)
        return;
    // Cross plants (grass/flowers) are replaceable: the new block takes the
    // plant's own cell. Solid targets place against the aimed face.
    const uint8_t targetBlk = world.getBlock(g_lookAt.blockX + 0.5f,
                                             g_lookAt.blockY + 0.5f,
                                             g_lookAt.blockZ + 0.5f);
    const bool replacePlant = blockRenderKind(targetBlk) == RK_CROSS;
    if (replacePlant) {
        g_placeX = g_lookAt.blockX;
        g_placeY = g_lookAt.blockY;
        g_placeZ = g_lookAt.blockZ;
    } else {
        if (g_lookAt.faceX == 0 && g_lookAt.faceY == 0 && g_lookAt.faceZ == 0)
            return;
        g_placeX = g_lookAt.blockX + g_lookAt.faceX;
        g_placeY = g_lookAt.blockY + g_lookAt.faceY;
        g_placeZ = g_lookAt.blockZ + g_lookAt.faceZ;
    }
    if (g_placeY <= 0 || g_placeY >= CHUNK_H ||
        !world.isLoadedAt(g_placeX, g_placeY, g_placeZ))
        return;
    const uint8_t cell = world.getBlock(g_placeX + 0.5f, g_placeY + 0.5f, g_placeZ + 0.5f);
    if (cell != BLOCK_AIR && cell != BLOCK_WATER &&
        blockRenderKind(cell) != RK_CROSS)
        return;
    if (placementOverlapsPlayer(g_placeX + 0.5f, g_placeY + 0.5f, g_placeZ + 0.5f,
                                player.x, player.y, player.z))
        return;
    g_canPlace = true;
}
static bool doBreakBlock(ChunkManager& world) {
    refreshLookAt(world);
    if (!g_hasTarget) return false;

    const float tx = (float)g_lookAt.blockX + 0.5f;
    const float ty = (float)g_lookAt.blockY + 0.5f;
    const float tz = (float)g_lookAt.blockZ + 0.5f;
    uint8_t blk = world.getBlock(tx, ty, tz);
    if (blk == BLOCK_AIR) return false;

    bool ok = world.setBlock(tx, ty, tz, BLOCK_AIR);
    if (ok) {
        // Doors occupy two stacked cells with the same id — remove both.
        if (blockIsDoor(blk)) {
            if (world.getBlock(tx, ty + 1.0f, tz) == blk) world.setBlock(tx, ty + 1.0f, tz, BLOCK_AIR);
            if (world.getBlock(tx, ty - 1.0f, tz) == blk) world.setBlock(tx, ty - 1.0f, tz, BLOCK_AIR);
        }
        g_actionPulse = 0.35f;
        SDL_Log("[Mine] Block removed at (%d,%d,%d)", g_lookAt.blockX, g_lookAt.blockY, g_lookAt.blockZ);
        if (g_craftMgr) g_craftMgr->giveBlock(blk, 1);
    }
    return ok;
}

// ── Interactable blocks: doors swing, lamps toggle, switches drive lamps ────
static bool interactAt(ChunkManager& world) {
    if (!g_hasTarget) return false;
    const float tx = (float)g_lookAt.blockX + 0.5f;
    const float ty = (float)g_lookAt.blockY + 0.5f;
    const float tz = (float)g_lookAt.blockZ + 0.5f;
    const uint8_t blk = world.getBlock(tx, ty, tz);

    if (blockIsDoor(blk)) {
        const uint8_t to = doorToggled(blk);
        world.setBlock(tx, ty, tz, to);
        if (world.getBlock(tx, ty + 1.0f, tz) == blk) world.setBlock(tx, ty + 1.0f, tz, to);
        if (world.getBlock(tx, ty - 1.0f, tz) == blk) world.setBlock(tx, ty - 1.0f, tz, to);
        SDL_Log("[Build] Door %s", blockIsSolid(to) ? "closed" : "opened");
        return true;
    }
    if (blk == BLOCK_LAMP_ON || blk == BLOCK_LAMP_OFF) {
        world.setBlock(tx, ty, tz, blk == BLOCK_LAMP_ON ? BLOCK_LAMP_OFF : BLOCK_LAMP_ON);
        return true;
    }
    if (blk == BLOCK_SWITCH_ON || blk == BLOCK_SWITCH_OFF) {
        const bool on = blk == BLOCK_SWITCH_OFF;   // state after the flip
        world.setBlock(tx, ty, tz, on ? BLOCK_SWITCH_ON : BLOCK_SWITCH_OFF);
        int flipped = 0;
        for (int dx = -6; dx <= 6; dx++)
        for (int dy = -4; dy <= 4; dy++)
        for (int dz = -6; dz <= 6; dz++) {
            const float bx = tx + dx, by = ty + dy, bz = tz + dz;
            const uint8_t b = world.getBlock(bx, by, bz);
            if (on  && b == BLOCK_LAMP_OFF) { world.setBlock(bx, by, bz, BLOCK_LAMP_ON);  flipped++; }
            if (!on && b == BLOCK_LAMP_ON)  { world.setBlock(bx, by, bz, BLOCK_LAMP_OFF); flipped++; }
        }
        SDL_Log("[Build] Switch turned %s — %d lamp(s)", on ? "ON" : "OFF", flipped);
        return true;
    }
    return false;
}

// ── Place block from selected hotbar slot (precise face placement) ──────────
// Build mode: every material stays stocked and placing never consumes items.
// Survival (consumption + persistence) comes later.
static constexpr bool BUILD_MODE = true;

static bool doPlaceBlock(ChunkManager& world) {
    refreshLookAt(world);
    // Tapping a door / lamp / switch interacts instead of placing.
    if (interactAt(world)) { g_actionPulse = 0.35f; return true; }
    updatePlacementPreview(world);
    if (!g_craftMgr || !g_hasTarget || !g_canPlace) return false;

    int sel = g_craftMgr->inventory().selectedSlot();
    const Craft::ItemSlot& s = g_craftMgr->inventory().slot(sel);
    if (s.item == Craft::ItemID::None || s.count == 0) return false;
    const Craft::ItemInfo& info = Craft::itemInfo(s.item);
    if (info.placeAs == BLOCK_AIR) return false;

    // Placement position = hit block + face normal
    float px = g_placeX + 0.5f;
    float py = g_placeY + 0.5f;
    float pz = g_placeZ + 0.5f;

    // Check: don't place inside the player
    if (placementOverlapsPlayer(px, py, pz, player.x, player.y, player.z)) {
        return false;
    }

    // Target cell must be air, water or a replaceable cross plant — placing
    // into water swaps just that voxel (no fluid sim, no seepage).
    const uint8_t cell = world.getBlock(px, py, pz);
    if (cell != BLOCK_AIR && cell != BLOCK_WATER &&
        blockRenderKind(cell) != RK_CROSS) return false;

    // Oriented blocks: the item's placeAs is the base variant.
    uint8_t place = (uint8_t)info.placeAs;
    if (place == BLOCK_STAIR_WOOD_PX || place == BLOCK_STAIR_STONE_PX) {
        // Stairs ascend away from the player so walking forward climbs them.
        const float dx = sinf(player.yaw), dz = cosf(player.yaw);
        int dir;                                   // 0=PX 1=NX 2=PZ 3=NZ
        if (fabsf(dx) > fabsf(dz)) dir = dx > 0 ? 0 : 1;
        else                       dir = dz > 0 ? 2 : 3;
        place = (uint8_t)(place + dir);
    } else if (place == BLOCK_DOOR_NS) {
        // Panel spans across the walking direction.
        const float dx = sinf(player.yaw), dz = cosf(player.yaw);
        place = (fabsf(dx) > fabsf(dz)) ? BLOCK_DOOR_WE : BLOCK_DOOR_NS;
    }

    // Doors are two blocks tall — the cell above must be free as well.
    const bool isDoor = blockIsDoor(place);
    if (isDoor) {
        if (g_placeY + 1 >= CHUNK_H) return false;
        const uint8_t above = world.getBlock(px, py + 1.0f, pz);
        if (above != BLOCK_AIR && above != BLOCK_WATER &&
            blockRenderKind(above) != RK_CROSS) return false;
        if (placementOverlapsPlayer(px, py + 1.0f, pz, player.x, player.y, player.z))
            return false;
    }

    bool ok = world.setBlock(px, py, pz, place);
    if (ok) {
        if (isDoor) world.setBlock(px, py + 1.0f, pz, place);
        g_actionPulse = 0.35f;
        if (!BUILD_MODE) g_craftMgr->inventory().removeItem(s.item, 1);
        SDL_Log("[Build] Placed %s at (%d,%d,%d)", info.name,
                g_placeX, g_placeY, g_placeZ);
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
                         Craft::CraftManager& craftMgr,
                         UI::InventoryPanel& inventoryPanel,
                         Craft::CraftingUI& craftingUI,
                         UI::SettingsPanel& settingsPanel,
                         Graphics::GraphicsSettingsManager& graphicsSettings) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (settingsPanel.isOpen()) {
            if (ev.type == SDL_KEYDOWN && (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_p)) { settingsPanel.close(); continue; }
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) { settingsPanel.handleClick(inputX(ev.button.x), inputY(ev.button.y), screenW, screenH, graphicsSettings); continue; }
            if (ev.type == SDL_FINGERUP) { settingsPanel.handleClick((int)(ev.tfinger.x*screenW),(int)(ev.tfinger.y*screenH),screenW,screenH,graphicsSettings); continue; }
            if (ev.type != SDL_QUIT) continue;
        }
        if (craftingUI.isOpen()) {
            if (ev.type == SDL_KEYDOWN &&
                (ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_c)) {
                craftingUI.close();
                setInventoryInputMode(false);
                continue;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                craftingUI.handleClick(inputX(ev.button.x), inputY(ev.button.y), craftMgr);
                continue;
            }
            if (ev.type == SDL_FINGERUP) {
                craftingUI.handleClick((int)(ev.tfinger.x * screenW),
                                       (int)(ev.tfinger.y * screenH), craftMgr);
                continue;
            }
            if (ev.type != SDL_QUIT) continue;
        }        // Touch HUD claims pointer events first (finger AND mouse) so taps
        // land on the joystick/buttons instead of the legacy handlers. The
        // inventory launcher button stays tappable in touch mode.
        if (g_touch.enabled() && !inventoryPanel.isOpen()) {
            int px = -1, py = -1;
            if (ev.type == SDL_FINGERDOWN) {
                px = (int)(ev.tfinger.x * screenW);
                py = (int)(ev.tfinger.y * screenH);
            } else if (ev.type == SDL_MOUSEBUTTONDOWN &&
                       ev.button.button == SDL_BUTTON_LEFT &&
                       ev.button.which != SDL_TOUCH_MOUSEID) {
                px = ev.button.x; py = ev.button.y;
            }
            if (px >= 0 && settingsPanel.pauseButtonHit(px, py, screenW, screenH)) { settingsPanel.toggle(); continue; }
            if (px >= 0 && inventoryPanel.mobileButtonHit(px, py, screenW, screenH)) {
                inventoryPanel.toggle();
                setInventoryInputMode(inventoryPanel.isOpen());
                continue;
            }
            if (g_touch.handleEvent(ev, screenW, screenH)) continue;
        }
        switch (ev.type) {
        case SDL_QUIT: running = false; break;

        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:
                if (inventoryPanel.isOpen()) { inventoryPanel.close(); setInventoryInputMode(false); }
                else running = false;
                break;
            case SDLK_p: if (!ev.key.repeat) settingsPanel.toggle(); break;
            case SDLK_e:
                if (!ev.key.repeat) { inventoryPanel.toggle(); setInventoryInputMode(inventoryPanel.isOpen()); }
                break;
            case SDLK_c:
                if (!ev.key.repeat) { craftingUI.toggle(); setInventoryInputMode(craftingUI.isOpen()); }
                break;
            case SDLK_w: case SDLK_UP:         keyW     = true;  break;
            case SDLK_s: case SDLK_DOWN:       keyS     = true;  break;
            case SDLK_a: case SDLK_LEFT:       keyA     = true;  break;
            case SDLK_d: case SDLK_RIGHT:      keyD     = true;  break;
            case SDLK_SPACE:                   keySpace = true;  break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: keyShift = true; break;
            // Spawn selector — jump to the nearest column of each biome
            // Number row selects the hotbar; Ctrl keeps the old debug spawns.
            case SDLK_1: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_OCEAN); else craftMgr.inventory().setSelected(0); break;
            case SDLK_2: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_BEACH); else craftMgr.inventory().setSelected(1); break;
            case SDLK_3: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_PLAINS); else craftMgr.inventory().setSelected(2); break;
            case SDLK_4: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_FOREST); else craftMgr.inventory().setSelected(3); break;
            case SDLK_5: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_DESERT); else craftMgr.inventory().setSelected(4); break;
            case SDLK_6: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_SNOWY); else craftMgr.inventory().setSelected(5); break;
            case SDLK_7: if (ev.key.keysym.mod & KMOD_CTRL) teleportToBiome(world, Biomes::BIOME_MOUNTAINS); else craftMgr.inventory().setSelected(6); break;
            case SDLK_8: if (ev.key.keysym.mod & KMOD_CTRL) teleportToCave(world); else craftMgr.inventory().setSelected(7); break;
            case SDLK_9: if (ev.key.keysym.mod & KMOD_CTRL) teleportToVillage(world); else craftMgr.inventory().setSelected(8); break;            case SDLK_F1:  if(g_weather) g_weather->forceWeather(WM::WeatherType::Clear,        -1.0f, g_worldTime ? g_worldTime->totalGameSecs() : 0.0); break;
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
            // T toggles the mobile touch HUD on PC (QA / mouse-driven play).
            case SDLK_t:
                if (!ev.key.repeat) {
                    g_touch.toggle();
                    setInventoryInputMode(inventoryPanel.isOpen());
                }
                break;
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
            if (inventoryPanel.isOpen()) {
                int ix = (int)(ev.tfinger.x * screenW), iy = (int)(ev.tfinger.y * screenH);
                inventoryPanel.handleMouseDown(ix, iy, craftMgr.inventory(), screenW, screenH);
                dragging = true; lastTouchX = ix; lastTouchY = iy;
                break;
            }
            dragging    = true;
            lastTouchX  = (int)(ev.tfinger.x * screenW);
            lastTouchY  = (int)(ev.tfinger.y * screenH);
            touchStartX = lastTouchX;
            touchStartY = lastTouchY;
            break;

        case SDL_FINGERMOTION:
            if (inventoryPanel.isOpen()) {
                inventoryPanel.handleMouseMove((int)(ev.tfinger.x * screenW), (int)(ev.tfinger.y * screenH), screenW, screenH);
                break;
            }
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
            if (inventoryPanel.isOpen()) {
                inventoryPanel.handleMouseUp(fx, fy, craftMgr.inventory(), screenW, screenH);
                dragging = false;
                break;
            }
            int ddx = fx - touchStartX;
            int ddy = fy - touchStartY;
            // Tap (< 15px movement) = interact; drag = look
            if (ddx*ddx + ddy*ddy < 15*15) {
                if (inventoryPanel.mobileButtonHit(fx, fy, screenW, screenH)) {
                    inventoryPanel.toggle();
                    setInventoryInputMode(inventoryPanel.isOpen());
                } else if (fx < screenW / 2)
                    doBreakBlock(world);
                else
                    doPlaceBlock(world);
            }
            dragging = false;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (inventoryPanel.isOpen()) {
                inventoryPanel.handleMouseDown(inputX(ev.button.x), inputY(ev.button.y), craftMgr.inventory(), screenW, screenH);
                break;
            }
            if (ev.button.button == SDL_BUTTON_LEFT)  doBreakBlock(world);
            if (ev.button.button == SDL_BUTTON_RIGHT) doPlaceBlock(world);
            break;

        case SDL_MOUSEBUTTONUP:
            if (inventoryPanel.isOpen())
                inventoryPanel.handleMouseUp(inputX(ev.button.x), inputY(ev.button.y), craftMgr.inventory(), screenW, screenH);
            break;

        case SDL_MOUSEWHEEL: {
            int sel = craftMgr.inventory().selectedSlot();
            sel = (sel - ev.wheel.y + Craft::Inventory::HOTBAR_SIZE)
                  % Craft::Inventory::HOTBAR_SIZE;
            craftMgr.inventory().setSelected(sel);
            break;
        }

        case SDL_MOUSEMOTION:
            if (inventoryPanel.isOpen()) inventoryPanel.handleMouseMove(inputX(ev.motion.x), inputY(ev.motion.y), screenW, screenH);
            else applyLook((int)(ev.motion.xrel * g_inputScaleX), (int)(ev.motion.yrel * g_inputScaleY));
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

    // Never synthesize mouse events from touches (or touches from the mouse):
    // the duplicated pointer made every Android tap fire twice (ghost places).
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOGE("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    Graphics::GraphicsSettingsManager graphicsSettings;
    graphicsSettings.initializePlatform();
    if (!graphicsSettings.load("graphics_settings.dat")) graphicsSettings.applyProfile(Graphics::GraphicsProfile::Medium);

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
#ifndef __ANDROID__
    const int aaQuality = (int)graphicsSettings.data().antiAliasing;
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, aaQuality >= 2 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, aaQuality >= 3 ? 4 : aaQuality >= 2 ? 2 : 0);
#endif

    int initialWindowW = WINDOW_WIDTH, initialWindowH = WINDOW_HEIGHT;
    if (const char* w = SDL_getenv("VOXEL_WINDOW_W")) initialWindowW = SDL_atoi(w);
    if (const char* h = SDL_getenv("VOXEL_WINDOW_H")) initialWindowH = SDL_atoi(h);
    SDL_Window* window = SDL_CreateWindow(
        "Voxel Engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        initialWindowW, initialWindowH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
#ifdef __ANDROID__
        | SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
    );

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
    SDL_GL_SetSwapInterval(graphicsSettings.data().vsync ? 1 : 0);
    int startupLogicalW=0,startupLogicalH=0,startupFramebufferW=0,startupFramebufferH=0;
    SDL_GetWindowSize(window, &startupLogicalW, &startupLogicalH);
    SDL_GL_GetDrawableSize(window, &startupFramebufferW, &startupFramebufferH);
    LOGI("[Resolution] window=%dx%d framebuffer=%dx%d renderScale=100%% dynamic=off fbo=default",
         startupLogicalW, startupLogicalH, startupFramebufferW, startupFramebufferH);

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
    const int anisoQuality = (int)graphicsSettings.data().anisotropic;
    atlas.setAnisotropic(anisoQuality == 1 ? 2.0f : anisoQuality == 2 ? 4.0f : anisoQuality >= 3 ? 8.0f : 1.0f);
    g_buildPreview.init(atlas.texture(), atlas.tileSizeUV(), atlas.texelUV());

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
    // World seed — VOXEL_SEED overrides for testing seed variety.
    unsigned worldSeed = 12345u;
    if (const char* sv = SDL_getenv("VOXEL_SEED"))
        worldSeed = (unsigned)SDL_atoi(sv);
    ChunkManager world(worldSeed);
    world.setViewDistance(graphicsSettings.data().renderDistance);

    // ── World Time ─────────────────────────────────────────────────────────
    WT::TimeManager worldTime;
    // Persist the calendar (day/season/year continue across sessions) but
    // always resume at 10:00 so every session starts in daylight.
    worldTime.load("world_time.dat");
    worldTime.normalizeToHour(10);

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
    // Perf/test: VOXEL_WEATHER=<type> forces a weather (0 = Clear) so QA
    // screenshots aren't taken through fog and rain.
    if (const char* wv = SDL_getenv("VOXEL_WEATHER"))
        weather.forceWeather((WM::WeatherType)SDL_atoi(wv), -1.0f,
                             worldTime.totalGameSecs());
    g_weather = &weather;  // expose to key handlers
    g_worldTime = &worldTime;  // expose game time to key handlers

    // ── Dynamic Seasons ────────────────────────────────────────────────────
    SN::SeasonManager seasons;
    seasons.load("world_season.dat");
    // QA: VOXEL_SEASON=<0..3.99> pins the year phase (0.5=spring, 1.5=summer,
    // 2.5=autumn, 3.5=winter, fractions land inside the transitions).
    if (const char* sv = SDL_getenv("VOXEL_SEASON"))
        seasons.forcePhase((float)SDL_atof(sv));
    seasons.events().subscribe(SN::SeasonEvent::OnSeasonChanged,
        [](SN::Season s, void*) {
            SDL_Log("[Seasons] Now in %s", SN::SEASON_PROFILES[(int)s].name);
        });

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
    g_minimap.init();

    // ── Weather Particles (rain / snow visuals) ─────────────────────────────
    WeatherParticles weatherParticles;
    weatherParticles.init(WM::weatherConfig().maxParticles);
    const int particleQuality = (int)graphicsSettings.data().particles;
    const int rainQuality = (int)graphicsSettings.data().rainDensity;
    weatherParticles.setDensity(std::min(particleQuality, rainQuality) / 3.0f);

    // ── Rain Sound (prepared interface — no audio yet) ───────────────────────
    WM::RainSound rainSound;

    // ── Block selection outline ──────────────────────────────────────────────
    g_blockOutline.init();

    // ── Crafting system + hotbar ─────────────────────────────────────────────
    Craft::CraftManager craftMgr;
    UI::Hotbar          hotbar;
    UI::InventoryPanel  inventoryPanel;
    Craft::CraftingUI   craftingUI;
    UI::SettingsPanel  settingsPanel;
    Craft::loadCraft(craftMgr, "world_craft.dat");
    // Build mode: stock the full construction catalogue every launch (any
    // loaded survival inventory is replaced — survival returns later).
    // First 9 entries land on the hotbar.
    {
        static const Craft::ItemID kCatalog[] = {
            // Hotbar row
            Craft::ItemID::Plank,       Craft::ItemID::StoneBricks, Craft::ItemID::Glass,
            Craft::ItemID::Window,      Craft::ItemID::Door,        Craft::ItemID::WoodStairs,
            Craft::ItemID::Lamp,        Craft::ItemID::Bed,         Craft::ItemID::Sink,
            // Backpack rows
            Craft::ItemID::Stone,       Craft::ItemID::Cobblestone, Craft::ItemID::Bricks,
            Craft::ItemID::Marble,      Craft::ItemID::Basalt,      Craft::ItemID::Obsidian,
            Craft::ItemID::StoneStairs, Craft::ItemID::IronBars,    Craft::ItemID::Switch,
            Craft::ItemID::Torch,       Craft::ItemID::Wood,        Craft::ItemID::BirchWood,
            Craft::ItemID::Leaves,      Craft::ItemID::Dirt,        Craft::ItemID::Sand,
            Craft::ItemID::Gravel,      Craft::ItemID::Clay,        Craft::ItemID::SnowBlock,
            Craft::ItemID::Ice,         Craft::ItemID::PottedPlant, Craft::ItemID::RedFlower,
            Craft::ItemID::YellowFlower,Craft::ItemID::TallGrass,   Craft::ItemID::Bush,
        };
        craftMgr.inventory().clear();
        for (Craft::ItemID id : kCatalog) craftMgr.inventory().addItem(id, 64);
        SDL_Log("[Build] Creative catalogue loaded: %d materials",
                (int)(sizeof(kCatalog) / sizeof(kCatalog[0])));
    }
    g_craftMgr = &craftMgr;
    if (!Craft::runCraftTests())
        SDL_Log("[Craft] WARNING: some craft tests FAILED");
    else
        SDL_Log("[Craft] All tests passed.");

    setInventoryInputMode(false);
    LOGI("Controls: WASD=move, Space/Shift=fly, LMB=mine, RMB=place, E=inventory, ESC=close");
    LOGI("Hotbar: 1-9 select, wheel switch; Ctrl+1-9 keeps debug spawn shortcuts");

    // Spawn inside a safe forest position when the map-only mode is active.
    if (world.villageIsPlaced()) {
        float vx, vy, vz;
        world.getVillageCoords(vx, vy, vz);
        player.x = vx; player.y = vy; player.z = vz;
        player.vx = player.vy = player.vz = 0.0f;
        player.grounded = false;
        LOGI("[Village] Starter village at (%d, %d) - spawning at safe surface y=%.1f",
             world.villageOriginX(), world.villageOriginZ(), player.y);
    } else {
        float sx, sy, sz;
        if (world.findBiomeSpawn(Biomes::BIOME_FOREST, sx, sy, sz)) {
            player.x = sx; player.y = sy; player.z = sz;
            player.vx = player.vy = player.vz = 0.0f;
            player.grounded = false;
            LOGI("[Spawn] Map start inside forest at (%.0f, %.0f, %.0f)",
                 player.x, player.y, player.z);
        } else {
            LOGI("[Spawn] WARNING: no safe plains found; using default spawn");
        }
    }

    // Perf/test: VOXEL_GOTO=<biome index> spawns straight in that biome.
    if (const char* g = SDL_getenv("VOXEL_GOTO"))
        teleportToBiome(world, (uint8_t)SDL_atoi(g));
    // Perf/test: VOXEL_AT=x,z drops the player at world coordinates
    // (screenshot harness — surface height from the generator).
    if (const char* av = SDL_getenv("VOXEL_AT")) {
        float ax = 0.0f, az = 0.0f;
        if (SDL_sscanf(av, "%f,%f", &ax, &az) == 2) {
            const int h = WorldGen::terrainHeight(ax, az, worldSeed);
            player.x = ax; player.z = az;
            player.y = (float)((h > WATER_LEVEL ? h : WATER_LEVEL) + 2);
            player.vx = player.vy = player.vz = 0.0f;
            SDL_Log("[Spawn] VOXEL_AT (%.0f, %.0f, %.0f)",
                    player.x, player.y, player.z);
        }
    }
    // Perf/test: VOXEL_YAW=<radians> aims the camera (screenshot harness).
    if (const char* yv = SDL_getenv("VOXEL_YAW"))
        player.yaw = (float)SDL_atof(yv);
    // Perf/test: VOXEL_EYE=<blocks> lifts the camera; VOXEL_PITCH aims it down.
    if (const char* ev = SDL_getenv("VOXEL_EYE"))
        player.y += (float)SDL_atof(ev);
    if (const char* pv = SDL_getenv("VOXEL_PITCH"))
        player.pitch = (float)SDL_atof(pv);

    // Mobile HUD: auto-on when a touchscreen exists; VOXEL_TOUCH=1 or the T
    // key force it on PC (mouse drives the same joystick/buttons).
    g_touch.detectAtStartup(graphicsSettings.data().force_mobile_ui_in_editor);
    g_touch.setControlsMode(graphicsSettings.data().controlsMode);
    if (g_touch.enabled())
        LOGI("[Touch] Mobile HUD enabled (%d touch device(s))",
             SDL_GetNumTouchDevices());
    LOGI("[Graphics] profile=%s AA=%s Aniso=%.1fx renderDistance=%d fpsLimit=%d vsync=%d force_mobile_ui_in_editor=%d",
         Graphics::profileName(graphicsSettings.data().profile), Graphics::antiAliasingName(graphicsSettings.data().antiAliasing), atlas.anisotropicApplied(), graphicsSettings.data().renderDistance,
         graphicsSettings.data().fpsLimit, graphicsSettings.data().vsync ? 1 : 0,
         graphicsSettings.data().force_mobile_ui_in_editor ? 1 : 0);

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

        int logicalW, logicalH;
        SDL_GetWindowSize(window, &logicalW, &logicalH);
        int screenW, screenH;
        SDL_GL_GetDrawableSize(window, &screenW, &screenH);
        if (screenW <= 0 || screenH <= 0) { screenW = logicalW; screenH = logicalH; }
        g_inputScaleX = logicalW > 0 ? (float)screenW / (float)logicalW : 1.0f;
        g_inputScaleY = logicalH > 0 ? (float)screenH / (float)logicalH : 1.0f;

        handleEvents(running, screenW, screenH, world, craftMgr, inventoryPanel, craftingUI, settingsPanel, graphicsSettings);

        // Touch HUD: camera drag, held-attack repeat, button taps.
        g_touch.update(dt);
        if (!inventoryPanel.isOpen() && !craftingUI.isOpen()) {
            int ldx = 0, ldy = 0;
            g_touch.consumeLook(ldx, ldy);
            if (ldx || ldy) applyLook(ldx, ldy);
            if (g_touch.consumeBreakFire()) doBreakBlock(world);
            if (g_touch.consumePlaceFire()) doPlaceBlock(world);
            const int hbSlot = g_touch.consumeHotbarTap();
            if (hbSlot >= 0) craftMgr.inventory().setSelected(hbSlot);
        }

        if (!inventoryPanel.isOpen() && !craftingUI.isOpen() && !settingsPanel.isOpen()) updateMovement(dt, world);

        // ── Lava contact: heat vignette + buoyant push (HP system to come) ──
        static float lavaHeat = 0.0f, lavaTick = 0.0f;
        {
            const uint8_t feet  = world.getBlock(player.x, player.y + 0.2f, player.z);
            const uint8_t waist = world.getBlock(player.x, player.y + 0.9f, player.z);
            if (feet == BLOCK_LAVA || waist == BLOCK_LAVA) {
                lavaHeat = fminf(1.0f, lavaHeat + dt * 2.5f);
                if (player.vy < 1.4f) player.vy = 1.4f;    // lava spits you out
                lavaTick += dt;
                if (lavaTick >= 0.5f) {
                    lavaTick = 0.0f;
                    // Burn: -1 HP each half second (death handling pending —
                    // HP floors at 1 so the HUD bar reads the danger).
                    g_hp = fmaxf(1.0f, g_hp - 1.0f);
                    g_hurtCooldown = 6.0f;
                    SDL_Log("[Lava] Burn tick! hp=%.0f", g_hp);
                }
            } else {
                lavaHeat = fmaxf(0.0f, lavaHeat - dt * 1.1f);
                lavaTick = 0.0f;
            }
            // Slow regen once out of danger for a while.
            g_hurtCooldown = fmaxf(0.0f, g_hurtCooldown - dt);
            if (g_hurtCooldown <= 0.0f && g_hp < 20.0f)
                g_hp = fminf(20.0f, g_hp + dt * 0.4f);
        }

        // Debug: log player position for first 120 frames (2 seconds)
        if (frameNum <= 120 && frameNum % 10 == 0) {
            uint8_t belowBlock = world.getBlock(player.x, player.y - 0.1f, player.z);
            uint8_t feetBlock  = world.getBlock(player.x, player.y, player.z);
            SDL_Log("[Player] pos=(%.1f,%.1f,%.1f) vy=%.1f grounded=%d below=%d feet=%d chunks=%d",
                    player.x, player.y, player.z, player.vy, player.grounded,
                    belowBlock, feetBlock, world.loadedChunkCount());
        }

        // Update block selection raycast
        refreshLookAt(world);
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

            // Seasons drive the wind multiplier and the foliage tint; the
            // atlas re-upload only happens when the tint drifted (~2%).
            seasons.update(dt, cal);
            if (seasons.tintDirty()) {
                atlas.applySeason(seasons.tint());
                seasons.markTintApplied();
            }

            weather.update(dt, worldTime.totalGameSecs(), cal,
                           world.biomeAtWorld(player.x, player.z),
                           hourOfDay, player.y, 64.0f, seasons.windMult());
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
        hotbar.update(dt, craftMgr.inventory().selectedSlot());
        inventoryPanel.update(dt);
        if (g_actionPulse > 0.0f) g_actionPulse -= dt;

        // Update world (load/unload chunks, rebuild dirty meshes)
        world.setViewDistance(graphicsSettings.data().renderDistance);
        world.update(player.x, player.z, meshAttr);
        refreshLookAt(world);
        updatePlacementPreview(world);

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
        float tanY = tanf(0.5f * cameraFovY(screenW, screenH) * PI / 180.0f);
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

        // Draw block selection outline and face-aligned build preview (after world).
        if (g_hasTarget) {
            const float mining = g_touch.miningProgress();
            if (mining > 0.0f)
                g_blockOutline.drawCracks((float)g_lookAt.blockX, (float)g_lookAt.blockY,
                                          (float)g_lookAt.blockZ, mvp, mining);
            g_blockOutline.draw((float)g_lookAt.blockX, (float)g_lookAt.blockY,
                                (float)g_lookAt.blockZ, mvp);
        }

        if (g_hasTarget && g_craftMgr) {
            const Craft::ItemSlot& held = g_craftMgr->inventory().heldItem();
            if (Craft::itemInfo(held.item).placeAs != BLOCK_AIR)
                g_blockOutline.draw((float)g_placeX, (float)g_placeY, (float)g_placeZ, mvp, true, g_canPlace);
            g_buildPreview.draw((float)g_placeX, (float)g_placeY, (float)g_placeZ, Craft::itemInfo(held.item).placeAs, g_canPlace, mvp, time);
        }
        // ── 2D Overlays: hotbar (always) + debug panel (F12) ────────────────
        debugOverlay.beginFrame(screenW, screenH);
        const bool renderDiagnostics = SDL_getenv("VOXEL_RENDER_DEBUG") != nullptr || graphicsSettings.data().showFPS;
        if (renderDiagnostics) {
            char renderInfo[192];
            const char* rendererName = (const char*)pglGetString(GL_RENDERER);
            snprintf(renderInfo, sizeof(renderInfo), "Resolution: %dx%d  Framebuffer: %dx%d  Render Scale: %.0f%%",
                     logicalW, logicalH, screenW, screenH, 100.0f);
            debugOverlay.drawText(renderInfo, 12, 12, UI::UIColorPalette::text_primary, UI::UITypography::hint);
            snprintf(renderInfo, sizeof(renderInfo), "FPS: %.1f  Renderer: %.48s", fps, rendererName ? rendererName : "unknown");
            debugOverlay.drawText(renderInfo, 12, 26, UI::UIColorPalette::text_secondary, UI::UITypography::hint);
        }
        debugOverlay.setIconTexture(atlas.texture());

        // Heat vignette: pulsing red wash while burning, fading after.
        if (lavaHeat > 0.01f) {
            const float pulse = 0.80f + 0.20f * sinf(time * 9.0f);
            debugOverlay.drawRect(0, 0, screenW, screenH, 0xff3c00,
                                  0.34f * lavaHeat * pulse);
        }

        if (!inventoryPanel.isOpen()) {
            int cx = screenW / 2, cy = screenH / 2;
            unsigned cross = g_canPlace ? UI::UIColorPalette::success_color : (g_hasTarget ? UI::UIColorPalette::selected_color : UI::UIColorPalette::text_primary);
            if (g_actionPulse > 0.0f) cross = UI::UIColorPalette::warning_color;
            // Drop shadow keeps the crosshair readable over snow/sky.
            debugOverlay.drawRect(cx, cy - 10, 2, 8, 0x000000, 0.55f);
            debugOverlay.drawRect(cx, cy + 4, 2, 8, 0x000000, 0.55f);
            debugOverlay.drawRect(cx - 10, cy, 8, 2, 0x000000, 0.55f);
            debugOverlay.drawRect(cx + 4, cy, 8, 2, 0x000000, 0.55f);
            debugOverlay.drawRect(cx - 1, cy - 11, 2, 8, cross, 0.95f);
            debugOverlay.drawRect(cx - 1, cy + 3, 2, 8, cross, 0.95f);
            debugOverlay.drawRect(cx - 11, cy - 1, 8, 2, cross, 0.95f);
            debugOverlay.drawRect(cx + 3, cy - 1, 8, 2, cross, 0.95f);
            if (g_hasTarget) {
                const uint8_t target = world.getBlock((float)g_lookAt.blockX + 0.5f,
                                                       (float)g_lookAt.blockY + 0.5f,
                                                       (float)g_lookAt.blockZ + 0.5f);
                const char* targetName = Materials::of(target).name;
                const char* actionHint = g_canPlace ? "TOQUE CURTO: COLOCAR" : "TOQUE LONGO: DESTRUIR";
                char targetLabel[128];
                std::snprintf(targetLabel, sizeof(targetLabel), "%s  |  %s", targetName, actionHint);
                const float labelScale = 1.15f;
                const int labelW = UI::UITheme::textWidth(targetLabel, labelScale) + 24;
                const int labelX = cx - labelW / 2;
                const int labelY = cy + 24;
                UI::roundedFill(debugOverlay, labelX + 2, labelY + 3, labelW, 30,
                                UI::UIColorPalette::shadow_color, 0.55f, 7);
                UI::roundedFill(debugOverlay, labelX, labelY, labelW, 30,
                                UI::UIColorPalette::background_primary, 0.86f, 7);
                debugOverlay.drawText(targetLabel, labelX + 12, labelY + 10,
                                      g_canPlace ? UI::UIColorPalette::success_color : UI::UIColorPalette::text_primary,
                                      labelScale);
            }
            if (g_touch.enabled() && (g_touch.miningGesture() || g_touch.miningProgress() > 0.0f)) {
                const int barW = 220, barH = 14;
                const int barX = screenW / 2 - barW / 2;
                const int barY = screenH / 2 + 42;
                UI::roundedFill(debugOverlay, barX, barY, barW, barH,
                                UI::UIColorPalette::shadow_color, 0.88f, 6);
                const int fill = (int)(barW * g_touch.miningProgress());
                if (fill > 0) UI::roundedFill(debugOverlay, barX, barY, fill, barH,
                                               UI::UIColorPalette::warning_color, 0.95f, 6);
                debugOverlay.drawText(g_touch.miningGesture() ? "MINING" : "HOLD",
                                      barX + 8, barY + 3,
                                      UI::UIColorPalette::text_primary, 1.10f);
            }
            hotbar.draw(debugOverlay, atlas, craftMgr.inventory(), screenW, screenH);
            if (g_touch.enabled()) {
                g_touch.draw(debugOverlay, screenW, screenH);
                g_hud.drawPlayerChip(debugOverlay, g_hp, 20.0f, g_food, 20.0f, screenH);

            }
        }
        inventoryPanel.drawMobileButton(debugOverlay, screenW, screenH);
        if (g_touch.enabled()) settingsPanel.drawPauseButton(debugOverlay, screenW, screenH);
        inventoryPanel.draw(debugOverlay, atlas, craftMgr.inventory(), screenW, screenH);
        craftingUI.draw(debugOverlay, craftMgr, screenW, screenH);
        settingsPanel.draw(debugOverlay, screenW, screenH, graphicsSettings.data(), fps);

        if (showDebug) {
            const WM::WeatherState& ws  = weather.state();
            const WT::CalendarState& cs = worldTime.calendar();
            int px = 10, py = 10;

            // Background panel
            debugOverlay.drawRect(px - 4, py - 4, 390, 392, 0x0d0d1e, 0.90f);

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

            // ── Seasons ───────────────────────────────────────────────────────
            {
                char buf[200];
                seasons.debugLine(buf, sizeof(buf));
                debugOverlay.drawText(buf, px, py, 0x9fe6a0);
                py += 13;

                const uint8_t pb = world.biomeAtWorld(player.x, player.z);
                const float hour01 = (float)cs.hour / 24.0f +
                                     (float)cs.minute / 1440.0f;
                snprintf(buf, sizeof(buf),
                         "Temp: %.1fC  Humid: %.0f%%  Chunk: %d,%d",
                         seasons.temperatureAt(pb, player.y, hour01,
                                               player.x, player.z),
                         seasons.humidityAt(pb, player.y, hour01),
                         (int)floorf(player.x / CHUNK_W),
                         (int)floorf(player.z / CHUNK_D));
                debugOverlay.drawText(buf, px, py, 0x9fe6a0);
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
        if (graphicsSettings.data().fpsLimit > 0) {
            const Uint32 targetMs = (Uint32)(1000 / graphicsSettings.data().fpsLimit);
            const Uint32 elapsedMs = SDL_GetTicks() - now;
            if (elapsedMs < targetMs) SDL_Delay(targetMs - elapsedMs);
        }
    }
    Craft::saveCraft(craftMgr, "world_craft.dat");
    graphicsSettings.save("graphics_settings.dat");
    worldTime.saveToLoadPath();      // calendar persists (resumes at 10:00)
    seasons.saveToLoadPath();
    weather.saveToLoadPath();
    weatherParticles.cleanup();
    g_blockOutline.cleanup();
    g_buildPreview.cleanup();
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
