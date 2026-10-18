# Roadmap

Planned work that is known but not yet scheduled.

## Bug fixes

### IOMatrix: out-of-bounds write to `functionToPinIn`

`IOMatrix::AssignFromParams()` (`src/iomatrix.cpp`) resets `functionToPinIn`
with `for (int i = 0; i < 8; i++)`, but the array is sized `LAST_IN` (6) in
`include/iomatrix.h`. The last two writes run past the end of the array into
adjacent static memory, which is undefined behaviour and can corrupt pin
assignments at startup. GCC reports it as
`iteration 6 invokes undefined behavior [-Waggressive-loop-optimizations]`.

**Fix:** bound the loop by `LAST_IN` instead of the literal `8`. Inherited
from upstream, so also worth reporting there.
