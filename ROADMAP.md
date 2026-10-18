# Roadmap

Planned work that is known but not yet scheduled.

## Bug fixes

None currently tracked. The `IOMatrix::AssignFromParams()` out-of-bounds
write listed here previously was fixed in #8 (loop bound corrected to
`LAST_IN`). That bug is inherited from upstream (damienmaguire/stm32-vcu),
so still worth reporting there.
