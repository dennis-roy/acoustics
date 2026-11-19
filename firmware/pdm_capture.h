#pragma once
#include <stdbool.h>
#include <stdint.h>

// Single owner on one core. Call open once and close after the run.
// Open starts Standard Mode. The caller drains and waits before entering Ultrasonic Mode.
// Read returns copied word count, zero while waiting, or -1 on a latched fault.
// Sample chronology in a word is MSB first. Flags follow stream v1.
bool pdm_capture_open(unsigned clock_pin, unsigned data_pin, bool sample_high);
bool pdm_capture_enter_ultrasonic(void);
int pdm_capture_read(uint32_t *dst, unsigned capacity);
uint32_t pdm_capture_status(void);
void pdm_capture_close(void);
