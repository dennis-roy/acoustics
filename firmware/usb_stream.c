#include "decimator.h"
#include "pdm_capture.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "stream_frame.h"
#include "tusb.h"
#ifdef ACOUSTIC_MIC
#include "hardware/clocks.h"

// At 2 MHz, 1,252 words allow 20 ms even across the GPIO handoff.
enum { STANDARD_WORDS = 1252, TRANSITION_WORDS = 12, FILTER_WARMUP_WORDS = 40 };
#endif

enum { BLOCKS = 100, ACQUISITION_FAULT = 2 };
static uint8_t frame[44 + STREAM_BLOCK * 2];
static int16_t pcm[STREAM_BLOCK];

static bool send_frame(uint8_t kind, uint8_t source, uint64_t run, unsigned seq,
                       unsigned first, uint32_t flags, unsigned count) {
    size_t remaining = stream_frame(frame, kind, source, run, seq, first,
                                    PCM_RATE_HZ, flags, pcm, count);
    const uint8_t *cursor = frame;
    uint64_t deadline = time_us_64() + 30000;
    while (remaining) {
        tud_task();
        if (!tud_cdc_connected() || time_us_64() >= deadline) return false;
        unsigned sent = tud_cdc_write(cursor, remaining);
        cursor += sent;
        remaining -= sent;
        tud_cdc_write_flush();
    }
    return true;
}

#ifdef ACOUSTIC_MIC
static bool collect(pdm_decimator *state, unsigned count, int16_t *output, uint64_t deadline) {
    for (unsigned filled = 0; filled < count;) {
        uint32_t words[32];
        unsigned wanted = count - filled;
        tud_task();
        if (!tud_cdc_connected() || time_us_64() >= deadline) return false;
        int n = pdm_capture_read(words, wanted < 32 ? wanted : 32);
        if (n < 0) return false;
        for (int i = 0; i < n; ++i) {
            if (state) {
                int16_t value = pdm_decimate_word(state, words[i]);
                if (output) output[filled] = value;
            }
            ++filled;
        }
    }
    return true;
}
#endif

static bool run_once(uint64_t run) {
#ifdef ACOUSTIC_MIC
    const uint8_t source = STREAM_MIC;
#else
    const uint8_t source = STREAM_SYNTHETIC;
    const int16_t tone[] = {0, 7071, 10000, 7071, 0, -7071, -10000, -7071};
#endif
    if (!send_frame(STREAM_START, source, run, 0, 0, 0, 0)) return false;
    uint32_t flags = 0;
    unsigned block = 0;
    bool transport_ok = true;
#ifdef ACOUSTIC_MIC
    bool opened = pdm_capture_open(ACOUSTIC_CLOCK_GPIO, ACOUSTIC_DATA_GPIO, ACOUSTIC_SAMPLE_HIGH);
    pdm_decimator state = {0};
    const uint64_t deadline = time_us_64() + 2000000ull + ACOUSTIC_SETTLE_WORDS * 10ull;
    if (!opened || !collect(NULL, STANDARD_WORDS, NULL, deadline) ||
        !pdm_capture_enter_ultrasonic() ||
        // Cover the joined RX FIFO, partial shifted word and DMA in flight before settling.
        !collect(NULL, ACOUSTIC_SETTLE_WORDS + TRANSITION_WORDS, NULL, deadline) ||
        !collect(&state, FILTER_WARMUP_WORDS, NULL, deadline))
        flags = ACQUISITION_FAULT;
#endif
    for (; !flags && block < BLOCKS; ++block) {
#ifdef ACOUSTIC_MIC
        if (!collect(&state, STREAM_BLOCK, pcm, deadline)) {
            flags = ACQUISITION_FAULT;
            break;
        }
#else
        for (unsigned i = 0; i < STREAM_BLOCK; ++i) pcm[i] = tone[i % 8];
#endif
        transport_ok = send_frame(STREAM_DATA, source, run, block + 1,
                                  block * STREAM_BLOCK, 0, STREAM_BLOCK);
        if (!transport_ok) break;
    }
#ifdef ACOUSTIC_MIC
    if (opened) {
        flags |= pdm_capture_status();
        pdm_capture_close();
    }
#endif
    return transport_ok && send_frame(STREAM_END, source, run, block + 1,
                                      block * STREAM_BLOCK, flags, 0);
}

int main(void) {
#ifdef ACOUSTIC_MIC
    if (!set_sys_clock_khz(128000, false)) return 1;
#endif
    if (!stdio_usb_init()) return 1;
    uint64_t run = 0;
    for (;;) {
        tud_task();
        if (tud_cdc_connected() && tud_cdc_available() && tud_cdc_read_char() == 'R') {
            if (!run_once(++run)) {
                tud_cdc_write_clear();
                while (tud_cdc_connected()) tud_task();
            }
        }
        tight_loop_contents();
    }
}
