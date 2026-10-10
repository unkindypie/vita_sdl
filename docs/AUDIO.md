# SDL audio

The shared backend predecodes the two upstream samples, `forest.ogg` ambience
and material-specific `footsteps.ogg`, using upstream's custom stb_vorbis decoder
and embedded cue markers. Both are 44,100 Hz and converted to native-endian
signed 16-bit stereo. No external Ogg/Vorbis library or JavaScript audio engine
is required. Upstream provides no separate combat/music sound assets here.

One looping ambient sample and 16 one-shot tracks feed SDL's playback callback
(44,100 Hz, stereo, 1,024-frame requested buffer). Gain is bounded and output
clipped. A 128-command single-producer/single-consumer lock-free queue carries
game-thread operations to the callback. Decoding/allocation/IO/logging happen
outside the callback; samples remain immutable until the device is closed.
`audio_dropped` exposes queue overflow; callback/signal counts prove PCM reached
the playback callback, not that physical speakers were audible.

Mute remains an upstream setting and is persisted. Focus/background loss pauses
the device; foreground resumes it. Missing/invalid samples or an unavailable
audio driver/device produce `AUDIO_ERROR` / `AUDIO_UNAVAILABLE` and visible silent
status; scene/gameplay still run. Close/join playback before scene teardown or
sample replacement on New Game. No microphone or audio recording is used.

Tests cover real Ogg decoding and cue extraction, stereo loop, clipping, cue
endpoints, mute, queue capacity/wraparound/overflow, driver failure, and device
pause/resume. Vita3K runs the same SDL backend and verifies nonzero PCM callbacks.
Physical Vita speaker/headphone and suspend/resume checks remain hardware-only.
