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

## Hardware session checklist

1. Check the existing CM5 project and the active USB device. Record firmware and receiver revisions, build output and exit status.
2. Run the simulated path first. Confirm that a complete run closes cleanly and that a corrupted or interrupted run becomes INVALID.
3. Verify board identity and electrical requirements before the wired capture step. Keep build and firmware flashing as separate operations. Obtain explicit flash approval for the selected artifact.
4. For live capture, verify the clock and data on the bench, then record capture duration, bytes, frame gaps, acquisition flags and filter response. Production verdicts require reference units and validated model-specific limits.
