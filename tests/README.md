# linuwux test suite

Minimal PE stubs that imitate the reflex / DenuvOwO loader handshake so
`liblinuwux.so` can be exercised under Wine without a full scene pack.

## What is covered

| Case | Marker | What it exercises |
|------|--------|-------------------|
| `modern_arm` | `reflex.dll` | Modern ARM leaf → modern KUSER + identity |
| `legacy_single` | `reflex.dll` | INIT → single → ARM → legacy-single KUSER |
| `legacy_dual` | `reflex.dll` | INIT → query leaves (promote dual) → ARM → legacy-dual KUSER |
| `denuvowo_force` | `DenuvOwO.ini` | Current hybrid behaviour: early force to modern |
| `no_marker` | *(none)* | Library stays silent (no banner, no protocol arm) |

Legacy cases deliberately use a **reflex** marker so the early DenuvOwO
modern-force does not fire. That is what real legacy Reflex packs do.
The `denuvowo_force` case documents the current hybrid path and will
need updating when hybrid handling is redesigned.

## Requirements

- `x86_64-w64-mingw32-gcc` (mingw-w64)
- `wine` or `wine64`
- A built `liblinuwux.so` (`./build.sh`, or already installed under `~/.local/lib`)

## Run

From the repo root:

```bash
./build.sh                  # if you do not already have liblinuwux.so
./tests/run.sh
```

Useful flags:

```bash
./tests/run.sh -v                  # full logs even on success
./tests/run.sh --keep-stage        # leave tests/.stage/ for inspection
./tests/run.sh --lib /path/to/liblinuwux.so
```

The runner:

1. Cross-compiles the four PE stubs into `tests/build/`
2. For each case, creates a private `WINEPREFIX` and stages the PE + marker
   under `drive_c/linuwux_test/` so Wine sees a real Windows path
   (`C:\linuwux_test\...`). The library's marker scan requires `argv[1]`
   to look like `X:\...`; a bare Unix path fails that check.
3. Runs the PE under Wine with `LINUWUX_DEBUG=1` and `LD_PRELOAD=liblinuwux.so`
4. Greps the combined log for the expected library messages

## Files

```
tests/
  README.md
  run.sh
  markers/          # empty 0-byte marker files (existence only)
  pe/
    common.h        # CPUID helpers + leaf constants
    modern_arm.c
    legacy_single.c
    legacy_dual.c
    no_marker.c
  build/            # produced by run.sh (gitignored if you want)
  .stage/           # produced by run.sh, cleaned unless --keep-stage
```

## Notes

- Markers are existence-only; the PE never loads them.
- Action leaves (ARM, INIT, KUSER, queries) are expected to come back
  zeroed when the library handles them. Static leaves return the
  spoofed identity for the active protocol.
- The PE stubs print the registers they observe; the runner’s pass/fail
  decision is based on the library’s own debug lines, not on those
  register values.
- Private `WINEPREFIX` per case so nothing touches your normal prefix.
