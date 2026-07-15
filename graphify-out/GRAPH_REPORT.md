# Graph Report - .  (2026-07-14)

## Corpus Check
- 58 files · ~60,369 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 725 nodes · 1169 edges · 48 communities (43 shown, 5 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 140 edges (avg confidence: 0.81)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Game Calendar & Time|Game Calendar & Time]]
- [[_COMMUNITY_Weather Particle Rendering|Weather Particle Rendering]]
- [[_COMMUNITY_Biome Generation System|Biome Generation System]]
- [[_COMMUNITY_Core Renderer & Shaders|Core Renderer & Shaders]]
- [[_COMMUNITY_Rain Sound & Particle Header|Rain Sound & Particle Header]]
- [[_COMMUNITY_Structure Generation|Structure Generation]]
- [[_COMMUNITY_GL Platform & Extensions|GL Platform & Extensions]]
- [[_COMMUNITY_Chunk Management|Chunk Management]]
- [[_COMMUNITY_Debug Overlay UI|Debug Overlay UI]]
- [[_COMMUNITY_World Time Manager|World Time Manager]]
- [[_COMMUNITY_World Save & Persistence|World Save & Persistence]]
- [[_COMMUNITY_Weather Manager|Weather Manager]]
- [[_COMMUNITY_Chunk Data Structure|Chunk Data Structure]]
- [[_COMMUNITY_Texture Atlas|Texture Atlas]]
- [[_COMMUNITY_Block Material System|Block Material System]]
- [[_COMMUNITY_Block Access & World Query|Block Access & World Query]]
- [[_COMMUNITY_Asset & Documentation Index|Asset & Documentation Index]]
- [[_COMMUNITY_Climate & Biome Weather|Climate & Biome Weather]]
- [[_COMMUNITY_Main Game Loop|Main Game Loop]]
- [[_COMMUNITY_Weather State Data|Weather State Data]]
- [[_COMMUNITY_Concurrency & Sync|Concurrency & Sync]]
- [[_COMMUNITY_Block Type Definitions|Block Type Definitions]]
- [[_COMMUNITY_Weather Properties Table|Weather Properties Table]]
- [[_COMMUNITY_Chunk Mesh Lifecycle|Chunk Mesh Lifecycle]]
- [[_COMMUNITY_Player Input & Actions|Player Input & Actions]]
- [[_COMMUNITY_Lighting & Cross-Chunk BFS|Lighting & Cross-Chunk BFS]]
- [[_COMMUNITY_Time Event System|Time Event System]]
- [[_COMMUNITY_Weather Transition Logic|Weather Transition Logic]]
- [[_COMMUNITY_Weather Event System|Weather Event System]]
- [[_COMMUNITY_Mesh Vertex Format|Mesh Vertex Format]]
- [[_COMMUNITY_World Decoration|World Decoration]]
- [[_COMMUNITY_Ore Generation|Ore Generation]]
- [[_COMMUNITY_Math Utilities|Math Utilities]]
- [[_COMMUNITY_Player State|Player State]]
- [[_COMMUNITY_Chunk Key & Hashing|Chunk Key & Hashing]]
- [[_COMMUNITY_Terrain Statistics|Terrain Statistics]]
- [[_COMMUNITY_Render Tests|Render Tests]]
- [[_COMMUNITY_Weather Debug & Types|Weather Debug & Types]]
- [[_COMMUNITY_Runtime Error Logs|Runtime Error Logs]]
- [[_COMMUNITY_IDE Plugin Config|IDE Plugin Config]]
- [[_COMMUNITY_Cave System Errors|Cave System Errors]]
- [[_COMMUNITY_Rain & Weather Errors|Rain & Weather Errors]]
- [[_COMMUNITY_WorldTime Error Log|WorldTime Error Log]]

## God Nodes (most connected - your core abstractions)
1. `main()` - 80 edges
2. `ChunkManager` - 65 edges
3. `WeatherParticles` - 43 edges
4. `WeatherManager` - 37 edges
5. `TimeManager` - 36 edges
6. `Chunk` - 36 edges
7. `Renderer` - 33 edges
8. `WeatherConfig` - 25 edges
9. `DebugOverlay` - 22 edges
10. `WorldSave` - 21 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `gl_load_extensions()`  [INFERRED]
  src/main.cpp → src/core/gl_ext.h
- `Texture Atlas (Procedural)` --conceptually_related_to--> `Atlas Upload Info (256x256 45 tiles)`  [INFERRED]
  assets/README.md → game_errors.txt
- `Renderer::init (OpenGL Init Sequence)` --conceptually_related_to--> `Renderer (OpenGL Subsystem)`  [INFERRED]
  voxel_debug.txt → game_errors.txt
- `main()` --calls--> `init`  [INFERRED]
  src/main.cpp → src/core/renderer.h
- `main()` --calls--> `beginFrame`  [INFERRED]
  src/main.cpp → src/core/renderer.h

## Import Cycles
- None detected.

## Communities (48 total, 5 thin omitted)

### Community 0 - "Game Calendar & Time"
Cohesion: 0.05
Nodes (26): CalendarState, dayOfMonth, dayOfWeek, hour, minute, month, period, season (+18 more)

### Community 1 - "Weather Particle Rendering"
Cohesion: 0.05
Nodes (46): SplashParticle, compileShader(), GLenum, GLuint, RainParticle, fastRand(), GLint, GLuint (+38 more)

### Community 2 - "Biome Generation System"
Cohesion: 0.08
Nodes (22): biomeAt(), BiomeGenerator, BiomeInfo, ambient, name, treeDensity, Biome, subBlock() (+14 more)

### Community 3 - "Core Renderer & Shaders"
Cohesion: 0.06
Nodes (35): compile_shader(), GLenum, GLuint, GLint, GLuint, Renderer, atlasTex, beginFrame (+27 more)

### Community 4 - "Rain Sound & Particle Header"
Cohesion: 0.06
Nodes (30): RainSound, current_, RainSoundState, active, pitch, thunderVol, volume, windVolume (+22 more)

### Community 5 - "Structure Generation"
Cohesion: 0.14
Nodes (30): bmask(), build(), buildBridge(), buildCabin(), buildCamp(), buildCastle(), buildDungeon(), buildFortress() (+22 more)

### Community 6 - "GL Platform & Extensions"
Cohesion: 0.08
Nodes (24): gl_load_extensions(), compile(), GLenum, GLuint, GLint, GLuint, Sky, aPos (+16 more)

### Community 7 - "Chunk Management"
Cohesion: 0.07
Nodes (18): GenRequest, ChunkManager, chunks, inFlight, lastGenMs, pending, qcv, qmutex (+10 more)

### Community 8 - "Debug Overlay UI"
Cohesion: 0.11
Nodes (26): buildFontTexture(), compileShader(), GLenum, GLuint, DebugOverlay, beginFrame, cleanup, drawRect (+18 more)

### Community 9 - "World Time Manager"
Cohesion: 0.09
Nodes (8): DayPeriod, Season, TimeManager, accum_, clock_, prev_, saveAccum_, savePath_

### Community 10 - "World Save & Persistence"
Cohesion: 0.11
Nodes (18): CK, BlockOverride, block, wx, wy, wz, unordered_map, vector (+10 more)

### Community 11 - "Weather Manager"
Cohesion: 0.13
Nodes (15): WeatherType, WeatherManager, ctrl_, currentIntensity_, currentType_, displayed_, expiresAt_, from_ (+7 more)

### Community 12 - "Chunk Data Structure"
Cohesion: 0.08
Nodes (22): Chunk, blockCount, blocks, caveRemoved, cx, cz, dirty, dominantBiome (+14 more)

### Community 13 - "Texture Atlas"
Cohesion: 0.17
Nodes (15): buildTiles(), clampb(), clearTile(), h32(), GLuint, paintOre(), paintSolid(), pnoise() (+7 more)

### Community 14 - "Block Material System"
Cohesion: 0.11
Nodes (17): BlockMaterial, bottom, breakable, category, hardness, id, lightEmit, name (+9 more)

### Community 15 - "Block Access & World Query"
Cohesion: 0.13
Nodes (18): getBlock, setBlock, biomeAtWorld, biomeHistogram, getBlock, getChunkAt, getNeighbors, lightStats (+10 more)

### Community 16 - "Asset & Documentation Index"
Cohesion: 0.14
Nodes (17): Assets Directory Structure, block_material.h, BlockMaterial Table, CC0 / Public Domain Asset License Policy, Texture Atlas (Procedural), texture_atlas.cpp, Atlas Upload Info (256x256 45 tiles), Lighting System (BFS Cross-Chunk) (+9 more)

### Community 17 - "Climate & Biome Weather"
Cohesion: 0.18
Nodes (12): applySeasonMod(), applyTempMod(), celsiusFor(), ClimateRegion, baseWeights, biomeName, humidity, temperature (+4 more)

### Community 18 - "Main Game Loop"
Cohesion: 0.15
Nodes (4): main(), calendar_, events_, events_

### Community 19 - "Weather State Data"
Cohesion: 0.15
Nodes (13): WeatherState, cloudCover, fogDensity, hasThunder, humidity, intensity, lightMult, rainRate (+5 more)

### Community 20 - "Concurrency & Sync"
Cohesion: 0.20
Nodes (10): atomic, condition_variable, deque, Frustum, mutex, vector, render, unordered_map (+2 more)

### Community 21 - "Block Type Definitions"
Cohesion: 0.21
Nodes (4): RenderKind, blockIsOpaque(), blockRenderKind(), findCaveSpawn

### Community 22 - "Weather Properties Table"
Cohesion: 0.17
Nodes (12): WeatherProps, cloudCover, fogDensity, lightMult, maxDurHours, maxIntensity, minDurHours, minIntensity (+4 more)

### Community 23 - "Chunk Mesh Lifecycle"
Cohesion: 0.24
Nodes (11): chash(), buildMesh, Chunk::Chunk(), cleanup, render, worldX, worldZ, FaceDef (+3 more)

### Community 24 - "Player Input & Actions"
Cohesion: 0.27
Nodes (9): applyLook(), doBreakBlock(), handleEvents(), teleportToBiome(), teleportToCave(), teleportToVillage(), updateMovement(), findBiomeSpawn (+1 more)

### Community 25 - "Lighting & Cross-Chunk BFS"
Cohesion: 0.29
Nodes (9): ChunkNeighbors, nx, nz, px, pz, computeAll(), computeBlock(), computeSky() (+1 more)

### Community 26 - "Time Event System"
Cohesion: 0.31
Nodes (4): EventCallback, TimeEvent, TimeEvents, listeners_

### Community 27 - "Weather Transition Logic"
Cohesion: 0.42
Nodes (5): WeatherType, lcg(), lcgf(), selectWeighted(), WeatherController

### Community 28 - "Weather Event System"
Cohesion: 0.28
Nodes (5): vector, WeatherEvents, listeners_, WeatherCallback, WeatherEvent

### Community 29 - "Mesh Vertex Format"
Cohesion: 0.22
Nodes (9): GLint, MeshAttribs, block, light, pos, sky, tile, uv (+1 more)

### Community 30 - "World Decoration"
Cohesion: 0.50
Nodes (7): decorate(), Biome, hash2(), oakTree(), rnd(), setIfAir(), spruceTree()

### Community 31 - "Ore Generation"
Cohesion: 0.29
Nodes (7): OreDef, block, maxY, minY, name, scale, threshold

### Community 32 - "Math Utilities"
Cohesion: 0.47
Nodes (4): computeMVP(), mat4_lookAt(), mat4_multiply(), mat4_perspective()

### Community 33 - "Player State"
Cohesion: 0.33
Nodes (6): Player, pitch, x, y, yaw, z

### Community 34 - "Chunk Key & Hashing"
Cohesion: 0.33
Nodes (4): ChunkKey, cx, cz, ChunkKeyHash

### Community 35 - "Terrain Statistics"
Cohesion: 0.33
Nodes (6): GenStats, caveRemoved, maxHeight, oreCounts, structures, structureType

### Community 36 - "Render Tests"
Cohesion: 0.60
Nodes (3): log_msg(), main(), make_mvp()

### Community 38 - "Runtime Error Logs"
Cohesion: 0.67
Nodes (3): Biome System, Structure System, Village System

## Knowledge Gaps
- **278 isolated node(s):** `@mimo-ai/plugin`, `locPos`, `locUV`, `locTile`, `locLight` (+273 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `Main Game Loop` to `Math Utilities`, `Weather Particle Rendering`, `Game Calendar & Time`, `Core Renderer & Shaders`, `Rain Sound & Particle Header`, `Weather Debug & Types`, `GL Platform & Extensions`, `Chunk Management`, `Debug Overlay UI`, `World Time Manager`, `Weather Manager`, `Texture Atlas`, `Block Access & World Query`, `Concurrency & Sync`, `Block Type Definitions`, `Player Input & Actions`?**
  _High betweenness centrality (0.424) - this node is a cross-community bridge._
- **Why does `ChunkManager` connect `Chunk Management` to `Chunk Key & Hashing`, `World Save & Persistence`, `Chunk Data Structure`, `Block Access & World Query`, `Concurrency & Sync`, `Block Type Definitions`, `Player Input & Actions`?**
  _High betweenness centrality (0.135) - this node is a cross-community bridge._
- **Why does `TimeManager` connect `World Time Manager` to `Game Calendar & Time`, `Time Event System`, `Main Game Loop`?**
  _High betweenness centrality (0.105) - this node is a cross-community bridge._
- **Are the 74 inferred relationships involving `main()` (e.g. with `gl_load_extensions()` and `beginFrame`) actually correct?**
  _`main()` has 74 INFERRED edges - model-reasoned connections that need verification._
- **What connects `@mimo-ai/plugin`, `locPos`, `locUV` to the rest of the system?**
  _279 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Game Calendar & Time` be split into smaller, more focused modules?**
  _Cohesion score 0.05411764705882353 - nodes in this community are weakly interconnected._
- **Should `Weather Particle Rendering` be split into smaller, more focused modules?**
  _Cohesion score 0.054693877551020405 - nodes in this community are weakly interconnected._