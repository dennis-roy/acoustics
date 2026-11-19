# Bench bring-up inventory

| Item | Evidence and next check |
|---|---|
| GitHub source | The implementation baseline contains README.md. Earlier Milestone 1 source was outside this repository. |
| CM5 | Raspberry Pi Compute Module 5 with IO Board. The earlier session reported Raspberry Pi OS 64-bit, Python 3.13.5 and a virtual environment with NumPy, SciPy and pyserial. Recheck those in the hardware-capable session. |
| USB | Pico 2 W to CM5 IO Board over a data-capable micro-USB cable. The earlier demo appeared as `/dev/ttyACM0`. Identify the active device by USB identity and use its stable `/dev/serial/by-id/` entry when available. |
| Firmware tools | The earlier CM5 session reported Pico SDK 2.3.1, Arm GCC 14.2.1, CMake 3.31.6 and Ninja 1.12.1. Recheck the SDK checkout, submodules and tool paths. Build for `pico2_w`. |
| Reusable Milestone 1 assets | Reported Pico C/C++ USB demo, simulated dual-stream firmware, binary framing and CRC, and CM5 `usb_receiver.py`. Exact CM5 project path and source revisions are pending. Locate them read-only before choosing code to reuse. A previous combined validation exited with status 1. Preserve the original projects and UF2. |
| Knowles board | User has the KAS-700-0164 unwired. Exact assembly revision and microphone marking are pending. The architecture brief names SPH0641LU4H-1. Treat that as a reference to reconcile with the actual board identification. |
| Required mic references | Obtain the Knowles KAS-700-0164 assembly pinout or schematic and the datasheet for the microphone physically fitted to that assembly. Match each reference to the observed marking and revision. Pinout, supply and logic levels, channel strap, sampling edge, and clock modes require verification before wiring. |

## Microphone capture checks

[DigiKey lists KAS-700-0164](https://www.digikey.in/en/products/detail/syntiant/KAS-700-0164/17878612) as an evaluation board for SPH0641LU4H-1. Use the [Knowles SPH0641LU4H-1 Rev B datasheet](https://media.digikey.com/pdf/Data%20Sheets/Knowles%20Acoustics%20PDFs/SPH0641LU4H-1.PDF) only after confirming the fitted part. Verify the assembly pinout, supply rails and I/O levels with the actual board documents. Strap SELECT to GND for `ACOUSTIC_SAMPLE_HIGH=0` or the verified microphone VDD for `=1`. The firmware samples within the driven half-cycle and enables the DATA input pull-down. Check the waveform and add external bias only if the assembly and measured loading warrant it.

Use exposed GPIOs 0â€“22 or 26â€“28. The [Pico 2 W board datasheet](https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf) lists GPIOs 23â€“25 and 29 as onboard functions. The USB images do not link or initialize wireless drivers. Assess supply noise, local bypass and cable coupling on the bench. Select ferrites or a production carrier from measured EMC and integration needs. Follow Knowlesâ€™ dielectric guidance for the microphone bypass capacitor. The non-wireless Pico 2 is a production candidate.

Check startup and reconnect with an oscilloscope. The live candidate runs 2 MHz for 20 ms, switches to 3.2 MHz, and discards the transition and the configured settling period. Verify both clock phases, data timing and the minimum settle period before accepting live measurements. The steady-state clock target is 50% duty within the microphoneâ€™s 48â€“52% Ultrasonic specification.

## Hardware session checklist

1. Check the existing CM5 project and the active USB device. Record firmware and receiver revisions, build output and exit status.
2. Run the simulated path first. Confirm that a complete run closes cleanly and that a corrupted or interrupted run becomes INVALID.
3. Verify board identity and electrical requirements before the wired capture step. Keep build and firmware flashing as separate operations. Obtain explicit flash approval for the selected artifact.
4. For live capture, verify the clock and data on the bench, then record capture duration, bytes, frame gaps, acquisition flags and filter response. Production verdicts require reference units and validated model-specific limits.
