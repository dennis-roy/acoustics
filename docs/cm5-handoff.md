# CM5 bench handoff

The baseline is [1fe0d70](https://github.com/dennis-roy/acoustics/commit/1fe0d700bab15e0f2d38b1fab34c5f463275f13f). GitHub Actions passed offline replay, reference checks, and both Pico 2 W UF2 builds. The live image built in CI uses placeholder GPIOs. Physical USB and microphone checks are pending. [Verified CI run](https://github.com/dennis-roy/acoustics/actions/runs/44339602475)

Use GPT-6.2 Sol Medium for the CM5 hardware session. Reuse the existing Python environment and Pico SDK checkout. Keep the known-good firmware and receiver in their existing directories. Obtain explicit approval before flashing or making system changes.

## 1. Verify offline on the CM5

From a separate checkout of this repository, activate the existing CM5 environment with NumPy and pyserial. Run the following commands from the checkout root. Use fresh output names for each replay.

```bash
python3 -c 'import numpy, serial'
python3 -m unittest discover -s tests -p 'test_*.py'
python3 bench/simulate.py tone cm5-tone.acou
python3 -m bench.receive --replay cm5-tone.acou --output cm5-tone-run
```

Expect PASS, 25,600 samples at 100,000 Hz, and both WAV and JSON files under `cm5-tone-run/run-0001`.

## 2. Build and check USB with the synthetic image

Set `PICO_SDK_PATH` to the existing SDK checkout. Verify the Arm compiler, CMake and SDK submodules before building.

```bash
cmake -S firmware -B build/synthetic -DPICO_BOARD=pico2_w -DACOUSTIC_ENABLE_MIC=OFF
cmake --build build/synthetic --target acoustic_synthetic -j2
sha256sum build/synthetic/acoustic_synthetic.uf2
```

Record the repository revision, SDK revision and UF2 digest. After explicit flash approval, use the named synthetic UF2. On the CM5, identify the Pico's actual USB CDC device and use its stable serial path if available.

```bash
ls -l /dev/serial/by-id/
python3 -m bench.receive --device "$PICO_SERIAL" --output cm5-usb-run
```

Set `PICO_SERIAL` to the verified device path before the command. Expect the same synthetic PASS and sample count as replay. Repeat with a fresh output directory. Check interrupted reception and recovery in a fresh session. For no START, check USB enumeration, data cable, permissions and whether another process has the device open. Review the recorded JSON before retrying.

## 3. Verify the microphone on the bench

Confirm the fitted microphone and assembly documentation, then verify the electrical conditions and capture settings in the [bring-up inventory](bring-up.md). Use a separate build directory and the live options in the [README](../README.md). Record the settings and live UF2 digest for approval. Record measured PDM clock and capture integrity, then check response, repeatability and sustained operation under the intended test conditions. Live product disposition remains HOLD pending validated limits and confirmed reference units.
