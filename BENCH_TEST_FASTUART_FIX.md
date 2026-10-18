# Bench test: FastUart DMA-race fix

For a Claude instance running on the bench machine, connected to the ZombieVerter
board via ST-Link (SWD) and a serial-to-USB adapter. This document is the full
test regime — read it end to end before doing anything, then work through it
step by step and report results back (see "Reporting results" at the end).

## What's being tested

`libopeninv`'s `Terminal::FastUart()` sends an "OK / Baud rate now ..." reply
over UART via DMA, then immediately reconfigures the USART's baud rate and
stop bits. `SendCurrentBuffer()` only waits for the DMA channel to be free
before starting a *new* transfer — it does not wait for the transfer it just
started to actually finish. So the baud/stopbits change can land while that
reply is still physically shifting out, corrupting its tail and/or desyncing
the link. This is why the ESP32 web interface permanently disabled fast UART
mode rather than deal with the corruption.

The fix adds a wait for both the DMA transfer-complete flag and the USART's
own TC (transmission complete) flag before touching the baud rate/stop bits.
It's currently local-only (not pushed anywhere, not yet proposed upstream to
`jsphuebner/libopeninv`) — this bench test is what decides whether it's good
enough to send upstream.

## Pinned state

This repo is a shared workspace, so the exact code is already sitting here —
no transfer needed. For the record:

- `zombieverter` (this repo): commit `7bf4ad6acc7f32db03f05f227714f24933fee63a`
  (`master`) — includes the IOMatrix out-of-bounds fix, BMW Z3 CAN option, BMS
  state CAN frame, and the throttle float-math change. None of those are what
  you're testing here; they're just what's currently on `master`.
- `libopeninv` submodule: branch `local/fastuart-dma-race-fix`, commit
  `fe73bf2f5ce11aa901022c9879b4f8f467a0a76c` — this is the actual fix under test.
  Check `git -C libopeninv log --oneline -1` matches before you start; if the
  workspace has moved on, `git -C libopeninv checkout fe73bf2f5ce11aa901022c9879b4f8f467a0a76c`
  to get back to this exact state.

A build already exists at `stm32_vcu.bin` / `stm32_vcu.hex` in the repo root,
built from exactly this state. If you want to rebuild (or confirm it matches):

```
make clean && make
```

Needs `gcc-arm-none-eabi` and `make` (install via the system package manager
if missing) plus the `libopencm3`/`libopeninv` submodules initialized
(`git submodule update --init`) — `libopencm3` also needs building once via
`make -C libopencm3 TARGETS=stm32/f1` if `libopencm3/lib/libopencm3_stm32f1.a`
doesn't already exist.

## Flashing — read this before you flash anything

Two things that will bite you if missed:

1. **Flash offset is `0x08001000`, not `0x08000000`.** The linker script
   (`stm32_vcu.ld`) reserves the first 4KB of flash for a bootloader
   (see `README.md`'s "Required Bootloader" section). Flashing the
   application at `0x08000000` will overwrite/corrupt that bootloader.
   Flashing the `.hex` file (not `.bin`) avoids this risk entirely, since
   Intel HEX records carry their own target addresses — OpenOCD's `program`
   command will write to the right place automatically. Only use the
   raw `.bin` if you explicitly pass `0x08001000` as the base address.

2. **The Makefile's `make flash` target won't work for your setup.** It's
   hardcoded for a parallel-port JTAG interface (`interface/parport.cfg`)
   and an Olimex board config — not ST-Link. Use OpenOCD directly instead:

   ```
   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
     -c "program stm32_vcu.hex verify reset exit"
   ```

   If `openocd` isn't installed, install it first. If you have `st-flash`
   (from `stlink-tools`) instead, it needs the raw `.bin` and the explicit
   offset: `st-flash write stm32_vcu.bin 0x08001000`.

3. One more from the existing Makefile's own comment, worth keeping: after
   flashing, use a full `reset`, not `resume` — the existing build process
   notes resume doesn't work correctly on this hardware.

Before flashing: no HV connected, motor not spinning, board just needs 12V
for the logic/UART side to test this.

## Serial connection parameters

The terminal lives on USART3. Before any `fastuart` command, connect at:

- **115200 baud, 8 data bits, 2 stop bits, no parity** (8N2) — note the 2
  stop bits; that's the actual default (`usart_set_stopbits(usart,
  USART_STOPBITS_2)` in `Terminal`'s constructor), not the more common 8N1.

After a `fastuart` command succeeds, the port switches to 1 stop bit (8N1)
at whichever baud you requested.

## Test regime

Do this in two phases — testing the fix alone isn't convincing on its own,
since the race is timing-dependent and might not always reproduce. Phase 1
establishes the bug is real and observable on this specific hardware; phase 2
confirms the fix resolves it.

### Phase 1 — confirm the bug exists (baseline, without the fix)

1. Temporarily revert the fix to get a baseline: `cd libopeninv && git stash`
   won't work since this commit *is* the fix — instead checkout the parent
   commit: `git -C libopeninv checkout 3a9c05946de6fab557ec099b9e47a48b946524c0`
   (the commit the fix was built on top of, i.e. `libopeninv` without the fix).
2. `make clean && make`
3. Flash this baseline build.
4. Connect a terminal at 115200 8N2. Run `fastuart 1`. Watch the reply
   character by character (a logic analyzer or scope on the TX line is ideal
   if available; otherwise watch closely for a mangled/truncated reply, or
   the next command failing after reconnecting at 921600).
5. Repeat `fastuart 1` / `fastuart 2` back to back at least 20-30 times
   (power-cycling or re-running `enableuart` between attempts as needed to
   get back to a known state). Note how often (if ever) you see corruption
   or desync. It may not reproduce every time — that's expected for a race.
6. Record what you saw (even "never reproduced in 30 tries" is useful data —
   note it, but treat phase 2 as the real test either way).

### Phase 2 — confirm the fix resolves it

1. Restore the fix: `git -C libopeninv checkout local/fastuart-dma-race-fix`
   (or the pinned commit hash above).
2. `make clean && make`
3. Flash this build.
4. Repeat the exact same procedure as phase 1, step 4-5: `fastuart 1` and
   `fastuart 2`, each at least 20-30 times back to back.
5. Expected result: the `"OK\r\nBaud rate now ..."` reply arrives intact
   every single time, and the very next command sent at the new baud rate
   works on the first try with no desync, for every repetition.

### What a failure looks like

- Reply text missing characters, truncated, or garbled near the end.
- Terminal appears to hang or echo nonsense after a `fastuart` command.
- The following command (at the new baud rate) gets no response, a garbled
  response, or requires toggling the port / reconnecting to recover.

If phase 2 shows any of this even once in 20-30 tries, that's a real finding
— don't average it away. Note the exact command, which repetition number,
and whatever you can see of the corrupted output.

## Reporting results

Update this file with a "## Results" section at the bottom: date, which
phase, how many repetitions, and what you observed (pass/fail per phase).
Commit that update locally (don't push — this is still pre-upstream). If
anything is ambiguous or you have to deviate from this procedure (missing
tool, different board revision, etc.), note that too rather than silently
improvising, so it's clear what was actually tested.
