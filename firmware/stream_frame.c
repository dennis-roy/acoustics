#include "stream_frame.h"
#include <string.h>

static void put_le(uint8_t *out, uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) {
        out[i] = (uint8_t)value;
        value >>= 8;
    }
}

size_t stream_frame(uint8_t *out, uint8_t kind, uint8_t source, uint64_t run,
                    uint32_t seq, uint64_t first, uint32_t rate, uint32_t flags,
                    const int16_t *pcm, unsigned count) {
    memcpy(out, "ACOU", 4);
    out[4] = 1;
    out[5] = kind;
    out[6] = source;
    out[7] = 0;
    put_le(out + 8, count * 2u, 4);
    put_le(out + 12, run, 8);
    put_le(out + 20, seq, 4);
    put_le(out + 24, first, 8);
    put_le(out + 32, rate, 4);
    put_le(out + 36, flags, 4);
    for (unsigned i = 0; i < count; ++i)
        put_le(out + 40 + i * 2, (uint16_t)pcm[i], 2);
    size_t end = 40 + count * 2u;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < end; ++i) {
        crc ^= out[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    put_le(out + end, ~crc, 4);
    return end + 4;
}
