# Offline parity matrix — version 0.3

Reference: Asciicker `8ff75d0c5a8d2745a8ad6a8a841dd31a46e81635`.
“Verified” means the specified observable behavior, not every possible scenario
or real-device certification. The same gameplay harness runs on Linux and ARM
in Vita3K via `--smoke --frames 600 --verify-gameplay`.

| System | Delivered behavior and explicit evidence |
| --- | --- |
| Scene/physics | Original terrain, meshes, water, sprites, movement, camera, jump/grounded collision; loaded scene and finite smoke checks, framebuffer inspection |
| Combat | Controller attack, sword equip and real melee HP damage against a loaded NPC; controller-triggered crossbow projectile with real shoot state/coordinates |
| Inventory/interactions | World item creation/pickup, script veto and story/description mutation, controller equip/unequip, drop into world; ownership/count assertions |
| Menus | In-game open/cancel, main-menu Continue, New Game request, safe world/JS reload in 600-frame launcher smoke, exit-confirmation cancellation; video toggles and mute retain upstream behavior |
| Configuration | Settings/talk-history write/read, temporary-file replacement, truncated-file rejection without partial apply; independent test profile, not user's settings |
| Scripting | All upstream `ak` method families, callback effects/removal, UTF-8, shared-buffer layout/bounds, limits/recovery, controller console submission, repeated context teardown |
| Audio | Actual forest/footstep Ogg decoding and cues, bounded mixing/overflow/mute/clipping; SDL callbacks produce nonzero PCM, pause/resume, no-driver silent fallback |
| Input/lifecycle | Keyboard/controller translation, touch and logical coordinates, focus reset, reconnect path; no L2/R2/stick-click or rear touch required |
| Packaging | ASCIIV001, version 00.30, “Asciicker Vita”, original icon, complete asset manifest and license/provenance notices; fresh install in isolated Vita3K storage |

The harness is intrusive diagnostics, not normal gameplay: it teleports the
player, creates a sword, damages an NPC and intentionally executes failing JS.
Expected exceptions (timeout, explicit throw, invalid bounds, stack/OOM) are
not evidence of errors during ordinary play. Performance runs omit this harness.

## Intentionally unavailable / not a missing port implementation

- **Persistent campaign/world save:** not implemented in the pinned upstream
  game. Continue resumes the current world; a fresh launch/New Game starts the
  bundled map. Only settings/talk presets persist. No save-game parity claim.
- **Profile/editor/multiplayer:** not part of the agreed offline runtime. The
  upstream placeholder Profile and fixed-grid zoom actions are hidden rather
  than presented as working features. Editor/debug exporters are not supported.
- **V8/Node/browser host APIs:** replaced by the documented synchronous QuickJS
  game API. No promise of arbitrary V8 embedding/DOM/filesystem compatibility.
- **GPU rewrite:** not adopted: scene work dominates measured CPU cost; existing
  glyph conversion is optimized and deterministic. Revisit with real-device
  measurements instead of assuming a GPU migration will improve the budget.

## Remaining external acceptance limits

Real Vita controls, suspend/resume, physical speaker/headphone output, memory
budget and stable 30 FPS still require a device. Emulator timings cannot prove
those. The upstream repository's MIT notice and exact asset provenance are
bundled; independent clearance of every upstream asset is not inferred from it.
Thus this is a tested offline port candidate, not a certified hardware/public
distribution release. See `VALIDATION.md` for recorded runs and measurements.
