#include "decimator.h"
#include <math.h>
#include <stdio.h>

// First-order pulse-density modulation of a known sine. Feed consecutive
// words through the same public routine used for DMA blocks.
static double measured_amplitude(double hz) {
    pdm_decimator state = {0};
    double error = 0, energy = 0;
    const unsigned discard = 200, length = 2000;
    for (unsigned i = 0; i < discard + length; ++i) {
        uint32_t word = 0;
        for (unsigned bit = 0; bit < 32; ++bit) {
            double phase = 6.283185307179586 * hz * (i * 32 + bit) / 3200000.0;
            error += 0.5 + 0.15 * sin(phase);
            unsigned high = error >= 1.0;
            error -= high;
            word = (word << 1) | high;
        }
        double pcm = pdm_decimate_word(&state, word) / 32768.0;
        if (i >= discard) energy += pcm * pcm;
    }
    return sqrt(2 * energy / length);
}

int main(void) {
    const double frequencies[] = {1000, 30000, 40000, 65000, 360000};
    for (unsigned i = 0; i < 5; ++i) {
        double amplitude = measured_amplitude(frequencies[i]);
        printf("%.0f Hz %.7f FS\n", frequencies[i], amplitude);
        if (!isfinite(amplitude)) return 1;
        if (i < 3 && fabs(amplitude - 0.30) > 0.015) return 1;
        if (i >= 3 && amplitude > 0.025) return 1;
    }
    return 0;
}
