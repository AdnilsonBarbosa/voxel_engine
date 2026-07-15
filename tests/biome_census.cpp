// Census of which biomes the generator actually produces over a large region,
// using the real biome.h logic and the real terrain height formula.
#include "biome.h"
#include "noise.h"
#include <cstdio>

int main() {
    const unsigned seed = 12345u;
    const int R = 750;                     // 1500 x 1500 world columns
    long tally[Biomes::BIOME_COUNT] = {0};
    long total = 0;

    for (int wz = -R; wz < R; wz += 2)
    for (int wx = -R; wx < R; wx += 2) {
        float n = Noise::fbm((float)wx, (float)wz, seed, 6, 0.004f, 2.0f, 0.5f);
        int h = 72 + (int)(n * 24.0f);
        if (h < 2) h = 2; if (h > 124) h = 124;
        Biomes::Biome b = Biomes::biomeAt((float)wx, (float)wz, h, seed);
        tally[b]++; total++;
    }

    printf("Biome census over %ld columns (seed %u):\n", total, seed);
    for (int i = 0; i < Biomes::BIOME_COUNT; i++)
        printf("  %-10s %6.2f%%\n", Biomes::info((Biomes::Biome)i).name,
               100.0 * tally[i] / total);
    return 0;
}
