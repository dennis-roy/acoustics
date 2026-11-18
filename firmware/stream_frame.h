#pragma once
#include <stddef.h>
#include <stdint.h>

enum { STREAM_START = 1, STREAM_DATA = 2, STREAM_END = 3 };
enum { STREAM_SYNTHETIC = 1, STREAM_MIC = 2, STREAM_BLOCK = 256 };
size_t stream_frame(uint8_t *out, uint8_t kind, uint8_t source, uint64_t run,
                    uint32_t seq, uint64_t first, uint32_t rate, uint32_t flags,
                    const int16_t *pcm, unsigned count);
