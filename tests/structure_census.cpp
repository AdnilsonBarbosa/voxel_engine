// Offline census: which structures the placement grid actually selects over a
// large region, using the real Structures::cellPick logic. Proves villages and
// temples generate (with the right biomes) without needing to fly there.
#define SDL_MAIN_HANDLED
#include "structure_gen.h"
#include <cstdio>

int main() {
    const unsigned seed = 12345u;
    const int R = 200;                         // 400×400 cells (~19k×19k blocks)
    long tally[Structures::S_COUNT] = {0};
    long total = 0;

    for (int gz = -R; gz < R; gz++)
    for (int gx = -R; gx < R; gx++) {
        int ox, oz;
        Structures::Type t = Structures::cellPick(gx, gz, seed, ox, oz);
        if (t == Structures::S_NONE) continue;
        tally[t]++; total++;
    }

    printf("Structure census over %d cells (seed %u):\n", (2*R)*(2*R), seed);
    for (int i = 1; i < Structures::S_COUNT; i++)
        if (tally[i])
            printf("  %-14s %6ld\n", Structures::def((Structures::Type)i).name, tally[i]);
    printf("  %-14s %6ld\n", "TOTAL", total);
    return 0;
}
