#include "pdm_capture.h"
#include "pdm_capture.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/regs/pio.h"
#include "hardware/sync.h"

enum { RING_WORDS = 8192, TRANSFERS = 0x0fffffff, SYS_HZ = 128000000,
       PIO_CYCLES = 4, STANDARD_HZ = 2000000, ULTRASONIC_HZ = 3200000 };
_Static_assert(SYS_HZ % (PIO_CYCLES * STANDARD_HZ) == 0, "Standard divider must be integral");
_Static_assert(SYS_HZ % (PIO_CYCLES * ULTRASONIC_HZ) == 0, "Ultrasonic divider must be integral");
static volatile uint32_t ring[RING_WORDS] __attribute__((aligned(32768)));
static PIO const port = pio0;
static int sm = -1, channel = -1;
static uint32_t produced(void);
static unsigned offset, clock_gpio, data_gpio;
static uint32_t consumed, faults;

static bool header_gpio(unsigned pin) {
    return pin <= 22 || (pin >= 26 && pin <= 28);
}

bool pdm_capture_open(unsigned clock_pin, unsigned data_pin, bool sample_high) {
    if (sm >= 0 || clock_pin == data_pin || !header_gpio(clock_pin) ||
        !header_gpio(data_pin) || clock_get_hz(clk_sys) != SYS_HZ)
        return false;
    sm = pio_claim_unused_sm(port, false);
    if (sm < 0) return false;
    channel = dma_claim_unused_channel(false);
    if (channel < 0 || !pio_can_add_program(port, &acoustic_pdm_program)) {
        if (channel >= 0) dma_channel_unclaim(channel);
        pio_sm_unclaim(port, sm);
        sm = channel = -1;
        return false;
    }
    offset = pio_add_program(port, &acoustic_pdm_program);
    clock_gpio = clock_pin;
    data_gpio = data_pin;
    gpio_init(clock_pin);
    gpio_put(clock_pin, false);
    gpio_set_dir(clock_pin, GPIO_OUT);
    pio_gpio_init(port, data_pin);
    gpio_pull_down(data_pin);
    pio_sm_config config = acoustic_pdm_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, clock_pin);
    sm_config_set_in_pins(&config, data_pin);
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv_int_frac(&config, SYS_HZ / (PIO_CYCLES * STANDARD_HZ), 0);
    pio_sm_init(port, sm, offset, &config);
    pio_sm_set_consecutive_pindirs(port, sm, data_pin, 1, false);
    pio_sm_set_consecutive_pindirs(port, sm, clock_pin, 1, true);
    // Hold physical CLK low through GPIO handoff, regardless of the SELECT strap.
    gpio_set_outover(clock_pin, GPIO_OVERRIDE_LOW);
    pio_gpio_init(port, clock_pin);
    port->fdebug = 1u << (PIO_FDEBUG_RXSTALL_LSB + sm);
    dma_channel_config dma = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&dma, false);
    channel_config_set_write_increment(&dma, true);
    channel_config_set_transfer_data_size(&dma, DMA_SIZE_32);
    channel_config_set_ring(&dma, true, 15);
    channel_config_set_dreq(&dma, pio_get_dreq(port, sm, false));
    consumed = faults = 0;
    dma_channel_configure(channel, &dma, (volatile void *)ring,
                          &port->rxf[sm], TRANSFERS, true);
    pio_sm_set_enabled(port, sm, true);
    gpio_set_outover(clock_pin, sample_high ? GPIO_OVERRIDE_NORMAL : GPIO_OVERRIDE_INVERT);
    return true;
}

bool pdm_capture_enter_ultrasonic(void) {
    if (pdm_capture_status()) return false;
    // Keep the state machine running across the divider change. Discard the transition in the caller.
    pio_sm_set_clkdiv_int_frac(port, sm, SYS_HZ / (PIO_CYCLES * ULTRASONIC_HZ), 0);
    if (pdm_capture_status()) return false;
    consumed = produced();
    return true;
}

static uint32_t produced(void) {
    return TRANSFERS - dma_channel_hw_addr(channel)->transfer_count;
}

uint32_t pdm_capture_status(void) {
    if (sm < 0) return 4;
    if (port->fdebug & (1u << (PIO_FDEBUG_RXSTALL_LSB + sm))) faults |= 1;
    if (!dma_channel_is_busy(channel)) faults |= 2;
    if (produced() - consumed >= RING_WORDS) faults |= 1;
    return faults;
}

int pdm_capture_read(uint32_t *dst, unsigned capacity) {
    if (pdm_capture_status()) return -1;
    uint32_t count = produced() - consumed;
    if (count > capacity) count = capacity;
    __dmb();
    for (uint32_t i = 0; i < count; ++i)
        dst[i] = ring[(consumed + i) & (RING_WORDS - 1)];
    __dmb();
    // Recheck before exposing the copy. DMA may have lapped us during it.
    if (pdm_capture_status()) return -1;
    consumed += count;
    return (int)count;
}

void pdm_capture_close(void) {
    if (sm < 0) return;
    gpio_set_outover(clock_gpio, GPIO_OVERRIDE_LOW);
    pio_sm_set_enabled(port, sm, false);
    dma_channel_abort(channel);
    dma_channel_unclaim(channel);
    gpio_disable_pulls(data_gpio);
    gpio_deinit(data_gpio);
    // SIO still has the low output value configured at open.
    gpio_set_function(clock_gpio, GPIO_FUNC_SIO);
    gpio_set_dir(clock_gpio, GPIO_OUT);
    gpio_set_outover(clock_gpio, GPIO_OVERRIDE_NORMAL);
    pio_remove_program(port, &acoustic_pdm_program, offset);
    pio_sm_unclaim(port, sm);
    sm = channel = -1;
}
