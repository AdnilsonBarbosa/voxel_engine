// Offline check: what fraction of the world floods, old formula vs new?
#include "noise.h"
#include <cstdio>

static constexpr int WATER_LEVEL = 62;

int main() {
    const unsigned seed = 12345u;
    const int N = 600;                 // sample 600x600 world columns
    long water_old = 0, water_new = 0, total = 0;

    for (int wz = -N/2; wz < N/2; wz++)
    for (int wx = -N/2; wx < N/2; wx++) {
        float n = Noise::fbm((float)wx, (float)wz, seed, 6, 0.004f, 2.0f, 0.5f);
        int hOld = 60 + (int)(n * 22.0f);   // before
        int hNew = 72 + (int)(n * 24.0f);   // after
        if (hOld <= WATER_LEVEL) water_old++;
        if (hNew <= WATER_LEVEL) water_new++;
        total++;
    }
    printf("Samples: %ld columns  (sea level y=%d)\n", total, WATER_LEVEL);
    printf("BEFORE  h=60+n*22 : %5.1f%% water\n", 100.0 * water_old / total);
    printf("AFTER   h=72+n*24 : %5.1f%% water\n", 100.0 * water_new / total);
    return 0;
}
