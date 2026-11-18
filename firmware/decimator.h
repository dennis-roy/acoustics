#pragma once
#include <stdint.h>

enum { PDM_FIR_TAPS = 129, PCM_RATE_HZ = 100000 };
typedef struct {
    uint32_t integral[3], previous[3];
    float history[PDM_FIR_TAPS];
    unsigned next;
} pdm_decimator;

// Zero-initialize once per run. Each 32 chronological bits yield one PCM sample.
int16_t pdm_decimate_word(pdm_decimator *state, uint32_t word);
