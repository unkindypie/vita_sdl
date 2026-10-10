# Trans Voice Training for PS Vita — Requirements

## 1. Document status

- **Status:** Initial planning baseline
- **Product name:** Trans Voice Training (working title)
- **Initial release:** MVP live voice-pitch graph
- **Platforms:** PS Vita and Linux development build
- **Implementation baseline:** C++17, SDL2, CMake, Docker, VitaSDK

This document defines product outcomes and acceptance criteria. It should remain
implementation-light enough to permit algorithm changes. Record newly agreed
scope here before building it.

## 2. Product intent

Provide private, immediate visual feedback that helps a user observe and
practice the fundamental frequency of their voice. The first version is a
focused pitch monitor, not a complete voice-training curriculum.

The app is intended as a practice aid. It must not claim that pitch alone
determines gender perception, promise a vocal outcome, or present itself as
medical diagnosis or treatment.

## 3. Users and core scenario

The primary user wants a portable, offline tool for voice practice. They launch
the app, allow/start microphone capture, sustain or speak a sound, see a stable
pitch trace with a current value, and stop without their audio being retained.

The Linux build serves development, algorithm evaluation, and accessibility of
testing; the Vita experience is the product baseline.

## 4. MVP scope

### 4.1 Startup and lifecycle

- **FR-001:** The app shall initialize video, input, and audio capture services
  and show a usable pitch-monitor screen.
- **FR-002:** The app shall allow the user to start and pause monitoring.
- **FR-003:** The app shall exit cleanly through an explicit control and handle
  the platform close/back event.
- **FR-004:** If no compatible microphone is available or capture initialization
  fails, the app shall remain responsive and show a useful error state.
- **FR-005:** Returning from a pause or platform lifecycle interruption shall
  either resume safely or clearly require the user to restart monitoring.

### 4.2 Audio capture

- **FR-010:** The app shall capture mono voice audio from a supported input
  source on both target platforms, using an SDL adapter where supported and a
  narrow platform adapter where Vita support requires it.
- **FR-011:** The selected device configuration (sample rate, channel count,
  sample format, and analysis frame size) shall be discoverable by the analysis
  pipeline and diagnostic UI/logging.
- **FR-012:** Capture and analysis shall use bounded memory and shall not block
  the real-time capture callback.
- **FR-013:** The MVP shall process audio in memory only. It shall not save or
  transmit microphone samples.
- **FR-014:** The UI shall distinguish at least: monitoring paused, listening,
  signal too quiet/unvoiced, pitch detected, and capture error.

### 4.3 Pitch analysis

- **FR-020:** For voiced monophonic input, the app shall estimate fundamental
  frequency and an associated confidence/quality value.
- **FR-021:** The initial supported display range shall be 65 Hz through 500 Hz.
  Values outside it shall be treated as out of range, not forced onto the graph.
- **FR-022:** Silence, low-energy input, noise, and low-confidence estimates
  shall not be displayed as a valid pitch.
- **FR-023:** The display value shall be smoothed enough to be readable while
  preserving intentional pitch movement. Raw estimates shall remain available
  for tests and diagnostics.
- **FR-024:** Pitch shall be displayable in hertz. Musical note/cents display is
  optional for the MVP and must not replace hertz.
- **FR-025:** On deterministic clean test signals from 80–400 Hz, the estimator
  shall report within the greater of ±2 Hz or ±2% after stabilization.
- **FR-026:** Algorithm parameters (range, confidence gate, analysis window, and
  smoothing) shall be named configuration values rather than unexplained magic
  numbers in rendering or capture code.

### 4.4 Live graph and UI

- **FR-030:** The primary screen shall show the current detected pitch in hertz
  and a scrolling time-history graph.
- **FR-031:** The graph shall have labeled pitch references and a visible time
  direction. Missing/unvoiced samples shall appear as gaps, not zero-pitch data.
- **FR-032:** The graph shall retain a bounded recent history. The initial target
  is 5–10 seconds and may be tuned after Vita performance testing.
- **FR-033:** The UI shall be legible at the Vita's native 960x544 resolution,
  including from normal handheld viewing distance.
- **FR-034:** Meaning shall not depend on color alone. Text/status and graph
  styling shall identify listening, valid, unvoiced, paused, and error states.
- **FR-035:** The Linux window shall preserve the Vita layout's aspect ratio or
  scale it predictably.
- **FR-036:** The render loop shall remain responsive while capture and analysis
  are active; slow analysis shall not stall input handling or clean shutdown.

### 4.5 Controls and guidance

- **FR-040:** On-screen hints shall identify start/pause and exit/back controls.
- **FR-041:** First-run or readily accessible guidance shall state that the tool
  visualizes pitch only, suggest comfortable-volume use, and avoid prescribing
  a universal gendered pitch range.
- **FR-042:** Any future target band shall be user-configurable and neutrally
  labeled. The app shall not grade a voice as valid/invalid or feminine/masculine.

## 5. Non-functional requirements

### Performance and reliability

- **NFR-001:** The MVP shall target a stable 60 Hz UI update on Vita, with a
  permitted fallback to a stable 30 Hz if profiling shows 60 Hz is impractical.
- **NFR-002:** Pitch feedback should update within 150 ms of usable captured
  audio, subject to the estimator window required for low frequencies.
- **NFR-003:** Analysis shall not lose responsiveness under continuous use for a
  15-minute manual practice session.
- **NFR-004:** Runtime buffers and graph history shall have fixed upper bounds;
  steady-state monitoring shall avoid heap allocation in the audio callback.
- **NFR-005:** Capture overruns/underruns and dropped analysis frames shall fail
  gracefully and be measurable in diagnostic builds.

### Portability and maintainability

- **NFR-010:** Shared domain, DSP, and graph-model code shall compile without
  Vita headers and be host-unit-testable.
- **NFR-011:** Platform-specific microphone and lifecycle code shall be isolated
  behind small interfaces.
- **NFR-012:** Both deliverables shall be produced through the documented Docker
  and Make workflow; host toolchain installation shall not be required.
- **NFR-013:** The app shall work fully offline after installation/build inputs
  are available.

### Privacy and safety

- **NFR-020:** The MVP shall not include accounts, analytics, networking, cloud
  processing, or audio recording.
- **NFR-021:** No raw audio or derived practice history shall survive app exit.
- **NFR-022:** User-facing copy shall frame results as informational feedback,
  not medical advice or a measure of identity.
- **NFR-023:** Guidance shall encourage stopping when practice causes pain or
  strain; the app shall not encourage extreme pitch targets.

## 6. Technical boundaries

The intended data flow is:

```text
microphone -> capture adapter -> bounded PCM buffer -> pitch estimator
           -> confidence gate -> display smoothing -> history model -> SDL UI
```

- Audio capture, pitch estimation, graph history, and rendering must be
  separable components.
- A fake/sample-generated capture source must be usable without a microphone.
- The pitch estimator must accept explicit PCM/sample-rate input and produce an
  estimate containing frequency, confidence, timestamp, and validity.
- Rendering consumes analysis results; it must never analyze raw audio.
- The first algorithm should favor predictable Vita performance and testability
  over a large dependency. Candidate algorithms must be compared using the same
  deterministic signal suite before selection.

## 7. Verification and acceptance

### Automated acceptance

- Linux Debug build completes in Docker.
- Vita Release build produces an installable VPK.
- Host-side unit tests run headlessly through CTest and cover the cases described
  in `AGENTS.md`.
- Deterministic estimator tests satisfy **FR-025** across multiple amplitudes,
  with explicit invalid results for silence and below-threshold signals.
- Buffer and pipeline tests demonstrate bounded behavior under producer/consumer
  rate mismatch and clean start/pause/stop transitions.

### Manual Linux acceptance

- App starts, displays the complete Vita-oriented layout, and exits cleanly.
- With a selected microphone, a sustained voice produces a plausible current
  hertz value and continuous graph trace; silence produces a gap/unvoiced state.
- Start/pause can be repeated without a hang or stale trace.
- Missing microphone and denied/unavailable capture show a useful error.

### Manual Vita acceptance

- VPK installs and launches on real hardware.
- Built-in or supported microphone capture works for at least 15 minutes without
  a crash, runaway latency, obvious memory growth, or input/UI lockup.
- Controls, labels, and trace are readable at native resolution.
- Pitch behavior is compared against the same voice or tone source used on Linux
  and any systematic sample-format/rate differences are documented.
- Suspending/resuming and exiting do not leave capture in a broken state.

## 8. Delivery plan

### Phase 0 — Feasibility and foundations

- Confirm SDL2 audio-capture support in the VitaSDK build and on real hardware.
- If needed, prove a minimal Vita-native capture adapter.
- Add CTest-based host test infrastructure and fake/generated audio input.
- Decide internal PCM format, sample rate strategy, and bounded-buffer design.

**Exit:** Captured sample levels can be observed on Linux and Vita, and the same
headless tests run through the Linux Docker workflow.

### Phase 1 — Pitch engine

- Implement and compare candidate estimators with generated tones, voice-like
  harmonics, silence, and noise.
- Add confidence gating, smoothing, timing metrics, and regression fixtures.
- Select defaults that satisfy accuracy and latency requirements on Vita.

**Exit:** The estimator meets **FR-025** in automated tests and processes live
Vita input within the performance budget.

### Phase 2 — MVP interface

- Build the current-value/status panel and bounded scrolling graph.
- Add start/pause, exit, error states, control hints, and safety/privacy copy.
- Integrate capture-to-render pipeline and perform Linux/Vita manual acceptance.

**Exit:** All MVP acceptance checks in section 7 pass or have documented,
approved exceptions.

### Phase 3 — Post-MVP candidates

Prioritize only after MVP feedback. Candidates include configurable reference
bands, note/cents display, calibration and input selection, pitch exercises,
session statistics stored locally with explicit consent, resonance-related
feedback, and additional language/accessibility options.

## 9. Explicitly out of scope for MVP

- Recording, playback, export, networking, accounts, telemetry, or cloud AI.
- Speech recognition, transcription, formant/resonance classification, or
  automatic gender classification.
- Lessons, scoring, streaks, social features, or prescriptive training plans.
- Claims of clinical accuracy or replacement of a qualified voice professional.

## 10. Open decisions and risks

Resolve these through a short spike or user decision; do not silently assume:

- **Vita capture API:** Does the packaged SDL2 port expose reliable capture, or
  is a Vita-native adapter required?
- **Input hardware:** Is the built-in Vita microphone the only MVP input, and
  how do headsets/accessories appear to the platform?
- **Estimator:** Which algorithm best balances low-frequency accuracy, latency,
  octave-error resistance, code size, and Vita CPU use?
- **Sample configuration:** Which device rates/formats are actually delivered on
  Vita, and should the app accept them directly or resample?
- **Calibration:** Is a user-adjustable noise floor necessary for the MVP?
- **UI dependencies:** Is SDL2 primitive rendering sufficient, or is a VitaSDK-
  compatible text/font solution required for readable labels?
- **Packaging identity:** Final app name, unique nine-character title ID,
  version, icon, and distributable assets must be chosen before release.
- **Licensing:** Any DSP, font, or UI dependency and bundled asset must have a
  license compatible with the intended distribution.

## 11. Requirement change log

When changing this document, add a short entry with the date, affected IDs, and
reason. Do not mark a requirement complete by deleting it; update its status or
move it to a later phase so the product history remains understandable.

- **2026-08-08:** Created initial MVP baseline for live pitch monitoring.
