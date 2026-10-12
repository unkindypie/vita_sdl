# JavaScript on Linux and Vita

Both targets embed official QuickJS **2026-06-04**, not V8, Python, a shell, or
the older QuickJS Vita fork. The original upstream `game_api.cpp` bootstrap is
used with native `akAPI_Call` registrations and one aligned 65,568-byte ordinary
ArrayBuffer. C++ and JS views share storage; bytes 0–65,535 carry parameters,
the last 32 bytes callback registration bits. Runtime teardown releases JS
views before the static backing storage is reused.

Supported `ak` methods: get/set Pos, Dir, Yaw, Name, Mount, Action, Move, Water,
Light; say, jump, isGrounded; onSay, onItem and onFrame. Pos/Dir/Yaw setters
reach physics and become observable after the next simulation update. Actions
retain upstream state-transition restrictions; an arbitrary action cannot
override death/fall just because a script requests it.

`assets/scripts/startup.js` installs a per-frame callback and counts frames in
`this.frames`; JavaScript genuinely runs during normal gameplay. Override or
extend with `--script PATH`. A New Game destroys the old context and callbacks,
loads a fresh world/inventory, then reruns startup and any command-line script.
`--no-js` is an explicit diagnostic baseline, not a supported parity mode. It
closes the runtime after normal scene/bootstrap loading, before the measured
frames: its three startup evaluations are outside frame timings, and no game
callbacks execute during the benchmark.

## Console

Open chat with Tab on Linux, or Select+Start on Vita. Enter a line beginning
with one backslash, such as `\ak.setWater(60)`, then submit with Tab/Enter on
desktop or Triangle on the controller keyboard. The backslash is stripped;
the code runs with `ak`, `akPrint` and persistent `this` (`akAPI_This`). Ordinary
chat is not JavaScript. `akPrint` and failures are logged; examples:

```js
ak.onFrame(function () { this.ticks = (this.ticks || 0) + 1; });
ak.onSay(function (text) { akPrint(text); return true; });
ak.onItem(function (action, story, kind, subkind, weight, description) {
    return { story_id: story, desc: description };
});
ak.onFrame(null); // unregister
```

onSay may veto with false. onItem accepts boolean, number, string, [story,desc],
object {story_id,desc}, null/undefined. The port fixes upstream array detection
and an overlapping story/text output offset. A throwing/timed-out callback is
logged and unregistered; the affected say/item operation fails closed.

## Bounds and deliberate limits

- Runtime allocation budget: 16 MiB; JS stack budget: 256 KiB.
- Evaluation: 50 ms; callbacks: 5 ms, checked cooperatively by QuickJS. Trusted
  bundled API initialization shares one total 1-second bootstrap deadline to
  accommodate cold ARM startup. Loaded game scripts and console evaluations do
  not receive that extended budget; teardown resets it.
  These are runaway-script protection, not hard real-time guarantees.
- Script files/source: 64 KiB. Shared-buffer string ranges and UTF-8 truncation
  are checked; native vector/scalar setters reject NaN/infinity and magnitudes
  over 1,000,000; invalid mount/action indices throw before entering the engine.
- Single game-thread evaluation. No worker/Atomics/SharedArrayBuffer API,
  quickjs-libc, `std`/`os`, filesystem bindings, module loader, timers, browser
  globals or asynchronous job-loop contract. Existing game callbacks and
  console scripts are synchronous. Do not assume browser/Node/V8 host APIs.
- Vita Date timezone is UTC. This is documented, not guessed from newlib.
- Scripts/assets are trusted local game content, not a hardened sandbox for
  adversarial code. The exposed bootstrap globals can be deliberately altered.

The shared Linux/Vita diagnostic harness verifies real bindings and callback
effects, Unicode, buffer bounds, interruption, stack/OOM recovery, controller
console execution and state reset. `SCRIPT_PROFILE` and per-frame `js_p50` /
`js_p95` track cost without computing a full heap census every frame.
