# acoustics

## Microphone-only bench prototype

Test Lucas TVS starter motors and alternators using the Knowles KAS-700-0164, Raspberry Pi Pico 2 W, and Compute Module 5 on its IO Board. The mic is on hand and awaiting wiring. This roadmap covers the bench prototype. Production thresholds and component-level diagnoses require evidence from real units.

**Next model switch is GPT-6.2 Astra High for tasks 4–6.** Tasks 1–3 have source and documentation in this repository. The protocol's golden frame was calculated independently. Python execution and physical bench checks are pending. Each numbered task has a primary file to create or extend. Reuse compatible code when available and record its actual path here. Add supporting build and test files alongside the implementation.

| Task | Model and effort | Primary file | Work and completion check |
|---|---|---|---|
| 1. Inventory and bring-up checklist | GPT-6.2 Sol Medium | [docs/bring-up.md](docs/bring-up.md) | Inspect existing repository assets and previously documented Milestone 1 work. Record reusable pieces, toolchain, exact mic board documents, unwired state, and outstanding hardware checks. Identify assets available only on the CM5 for the deployment session. |
| 2. Define the USB stream | GPT-6.2 Sol Medium | [protocol/stream.md](protocol/stream.md) | Specify byte order, format, rates, run identity, counters, integrity flags, CRC coverage, and restart behavior. Supply a byte-exact example shared by firmware and host tests. |
| 3. Simulate recordings | GPT-6.2 Sol Medium | [bench/simulate.py](bench/simulate.py) | Generate deterministic tone, silence, clipping, gap, corruption, and restart cases using the stream contract. Label every fixture synthetic. Check expected samples and counters offline. |
| 4. Capture PDM on the Pico | GPT-6.2 Astra High | firmware/pdm_capture.c | Verify board identity, supply and logic levels, clock and data edge before wiring guidance. Design clocking, PIO and DMA ownership for continuous acquisition. Detect overrun during USB stalls. Cross-compile for pico2_w and define on-bench clock and overrun checks. |
| 5. Convert PDM to PCM | GPT-6.2 Astra High | firmware/decimator.c | Select clock, decimation stages, filter coefficients and numeric widths against the analysis bandwidth. Test against a reference waveform, across block boundaries, at the passband edge and with out-of-band input. Record measured filter response separately from microphone performance. |
| 6. Define acoustic evidence and verdicts | GPT-6.2 Astra High | bench/evaluate.py | Implement signal-quality checks, spectral features, model-specific limits and decision precedence. Demonstrate synthetic PASS, FAIL and INVALID. Hold real-unit decisions until applicable limits are validated. Tie suspected components to verified fault evidence and retain an unknown outcome. |
| 7. Connect firmware to USB | GPT-6.2 Sol Medium | firmware/usb_stream.c | Integrate acquisition and decimation with framing, backpressure handling and a synthetic-source build. Compare frames with task 2's example and exercise disconnect and recovery. |
| 8. Receive and store a run on CM5 | GPT-6.2 Sol Medium | bench/receive.py | Decode partial reads, check CRC and continuity, record waveform and metadata, and invoke the evaluator. Replay every fixture and confirm interrupted captures stay invalid. |
| 9. Verify the integrated bench | GPT-6.2 Sol Medium | tests/test_bench.py | Run host regression and replay tests and cross-build both Pico targets. On hardware, verify synthetic transport first, then wired microphone timing, sustained capture and repeatability. Track pending physical checks separately. |
| 10. Package the CM5 handoff | GPT-6.2 Sol Low | docs/cm5-handoff.md | Record verified environment, build and run commands, expected outputs, recovery steps and open checks. Link each completed subtask from this README. Identify the commit and firmware artifact for the deployment session. |

### Model switches and execution

Run adjacent tasks with the same model together. Pause before task 4 for **Astra High**, before task 7 for **Sol Medium**, and before task 10 for **Sol Low**. Ask the user to switch at each boundary. If an Astra task finishes early and only routine work remains, request the downgrade immediately. Escalate a routine task only when a specific unresolved failure needs deeper reasoning. Record the smallest reproducer and the exact next check before switching.

Work through GitHub in this session. Keep existing working firmware and receiver intact. Use a hardware-capable session for CM5 execution. Obtain explicit approval before flashing the Pico or making destructive or system changes. Preserve artifacts and report what actually ran, including any unverified hardware checks. For diagnostic validation, collect model, test conditions and independently confirmed good and faulty units, and evaluate on held-out physical units.

### Generate offline fixtures

Run `python3 bench/simulate.py tone tone.acou` from the repository root. The script uses the standard library, checks the golden frame, and creates a new output file. Other cases are `silence`, `clipping`, `gap`, `corruption`, and `restart`. At the default rate, complete runs contain 25,600 synthetic samples. Silence and clipping are structurally complete but should exercise measurement-quality checks. Gap and corruption must invalidate the run. Restart must invalidate the incomplete first run and allow the second run to complete.
