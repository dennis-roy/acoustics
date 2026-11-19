# USB CDC acoustic stream v1

Each bench recording is one run of mono signed 16-bit PCM. Both ends use the following format over USB CDC ACM as a byte stream. Read and write boundaries carry no framing semantics. The host opens the configured serial device and treats its line speed as a CDC setting.

## Frame layout

All multibyte fields and PCM values are little-endian. Header size is 40 bytes. A four-byte CRC follows the payload.

| Offset | Size (bytes) | Field | Values |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `ACOU` |
| 4 | 1 | Version | 1 |
| 5 | 1 | Kind | 1 START, 2 DATA, 3 END |
| 6 | 1 | Source | 1 synthetic, 2 microphone |
| 7 | 1 | Reserved | Zero |
| 8 | 4 | Payload byte count | DATA uses an even count from 2 through 4096. START and END use zero. |
| 12 | 8 | Run ID | Nonzero opaque identifier, distinct for each run within a connection |
| 20 | 4 | Frame sequence | START is zero. Increment by one per frame, including END. |
| 24 | 8 | Sample index | START is zero. DATA identifies its first PCM sample. END gives total samples emitted. |
| 32 | 4 | Sample rate in Hz | Positive integer, constant across the run |
| 36 | 4 | Status flags | Bit 0 capture overrun, bit 1 clock or acquisition fault, bit 2 source interruption. All other bits reserved. |
| 40 | variable | Payload | DATA contains successive PCM samples from -32768 through 32767. |
| 40 + payload size | 4 | CRC | CRC-32/ISO-HDLC of the entire header and payload. Python equivalent is `zlib.crc32(header + payload)`. |

The sample-rate field describes the configured PCM rate. The simulator uses 100000 Hz by default as a provisional bench value. The live acquisition task verifies the actual clock and filter bandwidth. The receiver checks the announced rate against its configured capture recipe before using the waveform.

## Run lifecycle and integrity

1. The producer sends START, one or more DATA frames and END. Each run has one source, rate and ID. The producer starts a fresh run before sequence or sample-index overflow.
2. The next frame sequence must equal the previous sequence plus one. The next DATA index or END count must equal the number of samples received since START. Payload size divided by two is the sample count. Use a running count rather than padding gaps.
3. Nonzero status bits, unsupported fields, CRC failures, sequence or index jumps, timeouts, transport loss and premature EOF during an active run invalidate it. Latch faults through END if the producer can still send. A new START while a run is active invalidates the previous run. Reset the reader on disconnect. Repeat run IDs within one connection are invalid.
4. For byte-level recovery, search for magic and validate the bounded header and CRC before accepting a candidate. If a candidate fails validation, resume scanning one byte after its starting magic. Bound buffering using the 4140-byte maximum frame size. After an integrity fault, require a validated START to accept a subsequent run. For file replay, incomplete active runs at EOF are invalid. Live runs use configurable idle and run deadlines.
5. A run is structurally complete after a validated END with at least one sample. That outcome permits downstream measurement checks. Synthetic runs are always identified as such, and silence or clipping checks belong to the evaluator.

## Byte-exact example

DATA for synthetic run 1, sequence 1, sample index 0, 100000 Hz, zero status bits and samples `123, -456`. This frame requires a matching START and END in a recording.

`41434f5501020100040000000100000000000000010000000000000000000000a0860100000000007b0038fe95bf38a1`

## Bench USB request

After opening CDC with DTR asserted, the CM5 sends one ASCII `R` to request one run. The Pico waits between requests. A successful request streams 100 DATA frames of 256 samples each. A capture fault may end the run early with flags. If USB stalls during transmission, the host records an incomplete run and closes the port. Reopen the port before retrying. A new USB session starts receiver-side validation afresh.

## Production extensions

Version 1 carries signed 16-bit PCM for the microphone bench. A 24-bit PCM container can reduce quantization error when the decimator retains that precision. It cannot extend the microphone's acoustic overload point. Before releasing a new transport version, specify and test sample scaling, packed encoding, WAV representation, quantization, throughput and decoder compatibility. Keep v1 replay available for existing captures.

A future version should define timestamped tach events on a shared or calibrated device timebase, with sequence and integrity checks. If auxiliary channels enter scope, define channel IDs, rates and timestamp alignment explicitly. Current bench captures use only the microphone and do not expose tach timing. Reserve any new fields in a versioned contract once capture hardware and synchronization requirements are known.
