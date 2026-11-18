#include "decimator.h"

// Blackman-windowed inverse CIC response below the 50 kHz design cutoff.
// 400 kHz intermediate rate. Coefficients have unit DC gain.
static const float taps[PDM_FIR_TAPS] = {
    -2.272478663e-22f, -8.340890363e-7f, -4.821828393e-6f, -7.855885128e-6f,
    -6.553701859e-8f, 2.271969912e-5f, 4.765151821e-5f, 4.731988185e-5f,
    3.128561099e-7f, -8.252407115e-5f, -1.493614969e-4f, -1.326641918e-4f,
    -8.650913650e-7f, 1.974133238e-4f, 3.372585200e-4f, 2.854307461e-4f,
    1.934015774e-6f, -3.929887891e-4f, -6.509729230e-4f, -5.361556390e-4f,
    -3.865430626e-6f, 7.040222621e-4f, 1.143516662e-3f, 9.251682520e-4f,
    7.211598127e-6f, -1.175274053e-3f, -1.882708687e-3f, -1.503866199e-3f,
    -1.285697731e-5f, 1.863189253e-3f, 2.954341330e-3f, 2.337651397e-3f,
    2.225558465e-5f, -2.840043563e-3f, -4.469820214e-3f, -3.512915932e-3f,
    -3.791865942e-5f, 4.203768778e-3f, 6.584043510e-3f, 5.153267903e-3f,
    6.451437195e-5f, -6.100778141e-3f, -9.537083817e-3f, -7.457693443e-3f,
    -1.116456797e-4f, 8.780736360e-3f, 1.375643188e-2f, 1.079676890e-2f,
    2.020073475e-4f, -1.274115465e-2f, -2.013896151e-2f, -1.599168964e-2f,
    -4.011150488e-4f, 1.918242115e-2f, 3.100728126e-2f, 2.534599058e-2f,
    9.702341382e-4f, -3.195186583e-2f, -5.480312622e-2f, -4.864169897e-2f,
    -3.915354349e-3f, 7.310169047e-2f, 1.608074340e-1f, 2.301225145e-1f,
    2.564280889e-1f, 2.301225145e-1f, 1.608074340e-1f, 7.310169047e-2f,
    -3.915354349e-3f, -4.864169897e-2f, -5.480312622e-2f, -3.195186583e-2f,
    9.702341382e-4f, 2.534599058e-2f, 3.100728126e-2f, 1.918242115e-2f,
    -4.011150488e-4f, -1.599168964e-2f, -2.013896151e-2f, -1.274115465e-2f,
    2.020073475e-4f, 1.079676890e-2f, 1.375643188e-2f, 8.780736360e-3f,
    -1.116456797e-4f, -7.457693443e-3f, -9.537083817e-3f, -6.100778141e-3f,
    6.451437195e-5f, 5.153267903e-3f, 6.584043510e-3f, 4.203768778e-3f,
    -3.791865942e-5f, -3.512915932e-3f, -4.469820214e-3f, -2.840043563e-3f,
    2.225558465e-5f, 2.337651397e-3f, 2.954341330e-3f, 1.863189253e-3f,
    -1.285697731e-5f, -1.503866199e-3f, -1.882708687e-3f, -1.175274053e-3f,
    7.211598127e-6f, 9.251682520e-4f, 1.143516662e-3f, 7.040222621e-4f,
    -3.865430626e-6f, -5.361556390e-4f, -6.509729230e-4f, -3.929887891e-4f,
    1.934015774e-6f, 2.854307461e-4f, 3.372585200e-4f, 1.974133238e-4f,
    -8.650913650e-7f, -1.326641918e-4f, -1.493614969e-4f, -8.252407115e-5f,
    3.128561099e-7f, 4.731988185e-5f, 4.765151821e-5f, 2.271969912e-5f,
    -6.553701859e-8f, -7.855885128e-6f, -4.821828393e-6f, -8.340890363e-7f,
    -2.272478663e-22f,
};

int16_t pdm_decimate_word(pdm_decimator *s, uint32_t word) {
    for (unsigned group = 0; group < 4; ++group) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            // CIC sums and differences deliberately use modulo-2^32 arithmetic.
            s->integral[0] += (word >> 31) ? 1u : UINT32_MAX;
            s->integral[1] += s->integral[0];
            s->integral[2] += s->integral[1];
            word <<= 1;
        }
        uint32_t value = s->integral[2];
        for (unsigned stage = 0; stage < 3; ++stage) {
            uint32_t delay = s->previous[stage];
            s->previous[stage] = value;
            value -= delay;
        }
        // Signed CIC result is bounded by 8^3. Recover sign portably.
        int centered = (int)((value + 512u) & 2047u) - 512;
        s->history[s->next] = (float)centered;
        if (++s->next == PDM_FIR_TAPS) s->next = 0;
    }
    float pcm = 0;
    unsigned index = s->next;
    for (unsigned i = 0; i < PDM_FIR_TAPS; ++i) {
        if (index == 0) index = PDM_FIR_TAPS;
        pcm += taps[i] * s->history[--index];
    }
    pcm *= 64.0f;
    if (pcm >= 32767.0f) return INT16_MAX;
    if (pcm <= -32768.0f) return INT16_MIN;
    return (int16_t)(pcm >= 0 ? pcm + 0.5f : pcm - 0.5f);
}
