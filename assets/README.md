# Assets

```
assets/
 ├ textures/
 │   ├ blocks/     block face tiles (grass, dirt, stone, ores, …)
 │   ├ nature/     plants, foliage, water
 │   └ ui/         HUD / menu textures
 ├ models/         future entity / prop models
 ├ audio/          block/step/ambient sounds (see BlockMaterial.sound)
 └ fonts/          bitmap / TTF fonts
```

## Textures are procedural

The block texture atlas is **generated in code at runtime**
(`src/rendering/texture_atlas.cpp`) as 16×16 pixel-art tiles packed into a
single 256×256 GL texture. This means:

- **No image files to ship** and **zero copyright concerns** — nothing here is
  derived from Minecraft or any protected asset.
- One texture bind for the whole world → draw-call count is unchanged.
- Resolution is a one-line change: bump `Atlas::TILE_PX` to 32 or 64.

Drop real `.png` tiles into `textures/blocks/` later and load them into the
same atlas slots if you prefer hand-drawn art; the `BlockMaterial` table
(`src/world/block_material.h`) already maps every block face to a tile id.

Any real assets added here **must** be CC0 / public-domain or otherwise
licensed for commercial use.
