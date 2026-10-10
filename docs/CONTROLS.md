# Controls

For `make vita3k`, use the emulator's controller-emulation keys, **not** the Linux
column below: WASD/arrows move, Q/E rotate, X attacks, C jumps, Enter opens
inventory, Z equips, V picks up, Right Shift opens the menu. Click the game
window to focus it. Mouse drag is Vita front touch. See [emulator controls and
manual performance runs](VITA3K.md#interactive-play-and-manual-performance-checks).

| Action | Linux | Vita / SDL controller |
| --- | --- | --- |
| Move | WASD / arrows | Left stick; D-pad outside inventory/menu |
| Rotate camera | Q / E | L / R; right-stick X on desktop pads |
| Jump | Space | Circle / SDL B |
| Attack / crossbow | Enter | Cross / SDL A |
| Inventory | I | Start |
| Equip/use focused item | Y / mouse inventory interaction | Square / SDL X (press/release) |
| Pick nearby item | Mouse nearby-item list | Triangle / SDL Y, D-pad selection, release |
| Move/drop inventory item | Drag with mouse | Hold Square + D-pad; release outside grid to drop |
| In-game menu | F1 | Select / SDL Back |
| Chat/script entry | Tab | Select+Start opens controller text keyboard |
| Submit chat/script | Tab or Enter | Triangle while text keyboard is open |
| Cancel text/menu | Escape | Select/menu back navigation; touch controls |
| Exit | Ctrl+Q / window close / Exit menu | Exit menu, then Yes |

Chat beginning with one backslash runs JavaScript; ordinary chat does not.
The controller keyboard and front touch provide text entry without a hardware
keyboard. Rear touch, L2/R2 and stick-click buttons are never required. Vita
uses the engine keyboard, not a native IME overlay during normal gameplay.
Touch uses front-panel coordinates; mouse/touch are mapped through SDL logical
960x544 letterboxing. Inventory retains the upstream drag/drop behavior.

Upstream uses custom pad indices, not SDL button ordinals. Keep the explicit
mapping in `src/platform/input.cpp`. L/R supply Q/E camera commands; right-stick
X supplies engine camera torque. A 6,000-unit deadzone suppresses drift.
Disconnect/focus/background resets engine input. The launcher clears touch IDs
and pauses playback until foreground; real Vita suspend/resume is unverified.

Menus offer Continue (current world only), New Game (fresh world and JS state),
video perspective/blood, mute, controls, credits and confirmed Exit. Desktop
fullscreen is available; Vita is already fullscreen. Fixed-grid zoom and the
upstream empty Profile action are intentionally hidden. No campaign save exists
in the pinned upstream runtime; settings and talk presets are the persisted data.
