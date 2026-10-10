# Phase 2 controls

| Action | Linux keyboard | Vita / SDL controller |
| --- | --- | --- |
| Move | WASD or arrow keys | Left stick; D-pad outside inventory/menu |
| Rotate camera | Q / E | L / R shoulders or right stick horizontally |
| Jump | Space | Circle / SDL B |
| Attack (upstream behavior, parity unverified) | Existing engine keys | Cross / SDL A |
| Inventory (upstream behavior, parity unverified) | Existing engine keys | Start |
| Exit | Escape / window close | Select / SDL Back |

Square/Triangle and D-pad retain upstream inventory/item behavior where
applicable, but this preview does not claim full gameplay/menu parity. Touch,
text entry, controller remapping UI, and L2/R2/stick-click controls are outside
Phase 2. Rear touch must not be needed to move, rotate, jump, or exit.

Upstream uses custom pad indices, not SDL button ordinals. Keep the explicit
mapping in `src/platform/input.cpp`. L/R are mapped to keyboard-equivalent camera
commands; right-stick X feeds the upstream camera torque axes. A 6,000-unit
deadzone suppresses stick drift. Controller removal resets upstream pad state.

Focus loss/background pauses the launcher and clears engine input through
`OnFocus`; foreground restores it. Linux substitutes validate the portable
event path. Actual Vita suspend/resume remains unverified for this pass.
