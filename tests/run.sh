#!/usr/bin/env bash
# ============================================================
# linuwux PE test runner
#
# Builds the test stubs (mingw-w64), stages each case with the
# correct marker file, runs under Wine with liblinuwux.so, and
# checks LINUWUX_DEBUG logs for the expected protocol transitions.
#
# Usage (from repo root or tests/):
#   ./tests/run.sh
#   ./tests/run.sh --lib /path/to/liblinuwux.so
#   ./tests/run.sh -v --keep-stage
#
# Requirements:
#   x86_64-w64-mingw32-gcc
#   wine / wine64
#   liblinuwux.so (./build.sh, or pass --lib)
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PE_DIR="${SCRIPT_DIR}/pe"
MARKERS_DIR="${SCRIPT_DIR}/markers"
BUILD_DIR="${SCRIPT_DIR}/build"
STAGE_ROOT="${SCRIPT_DIR}/.stage"

LIB=""
KEEP_STAGE=0
VERBOSE=0

if [[ -t 1 ]]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED='' GREEN='' YELLOW='' CYAN='' BOLD='' RESET=''
fi

info()  { echo -e "${GREEN}==> $*${RESET}"; }
warn()  { echo -e "${YELLOW}WARNING: $*${RESET}" >&2; }
die()   { echo -e "${RED}ERROR: $*${RESET}" >&2; exit 1; }
header(){ echo -e "\n${CYAN}${BOLD}$*${RESET}"; }

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --lib <path>     Path to liblinuwux.so (default: dist/ or ~/.local/lib)
  --keep-stage     Leave per-test staging dirs under tests/.stage/
  -v, --verbose    Print full wine + library logs even on success
  -h, --help       Show this help
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --lib) LIB="${2:-}"; shift 2 ;;
        --keep-stage) KEEP_STAGE=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) usage ;;
        *) die "Unknown argument: $1" ;;
    esac
done

need() { command -v "$1" >/dev/null 2>&1 || die "'$1' is required but not found"; }

MINGW=""
for cand in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-gcc.exe; do
    if command -v "$cand" >/dev/null 2>&1; then
        MINGW="$cand"
        break
    fi
done
[[ -n "$MINGW" ]] || die "x86_64-w64-mingw32-gcc not found (install mingw-w64)"

WINE=""
for cand in wine64 wine; do
    if command -v "$cand" >/dev/null 2>&1; then
        WINE="$cand"
        break
    fi
done
[[ -n "$WINE" ]] || die "wine/wine64 not found"

if [[ -z "$LIB" ]]; then
    for cand in \
        "${REPO_ROOT}/dist/liblinuwux.so" \
        "${HOME}/.local/lib/liblinuwux.so" \
        "${REPO_ROOT}/liblinuwux.so"
    do
        if [[ -f "$cand" ]]; then
            LIB="$cand"
            break
        fi
    done
fi
[[ -n "$LIB" && -f "$LIB" ]] || die "liblinuwux.so not found — build with ./build.sh or pass --lib"

info "mingw  : $MINGW"
info "wine   : $WINE"
info "library: $LIB"

# ------------------------------------------------------------
header "Building PE stubs"
mkdir -p "$BUILD_DIR"

PE_SRCS=(modern_arm legacy_single legacy_dual no_marker)
for name in "${PE_SRCS[@]}"; do
    src="${PE_DIR}/${name}.c"
    out="${BUILD_DIR}/${name}.exe"
    [[ -f "$src" ]] || die "Missing $src"
    info "  $name.exe"
    "$MINGW" -O2 -Wall -Wextra -I"$PE_DIR" -o "$out" "$src" \
        || die "Compile failed: $name"
done

# ------------------------------------------------------------
PASS=0
FAIL=0

# stage_and_run <name> <marker-or-empty> [exe-basename] -- <patterns...>
# If exe-basename is omitted, name.exe is used. Patterns follow after --
stage_and_run() {
    local name="$1"
    local marker="$2"
    shift 2
    local exe_base="$name"
    if [[ $# -gt 0 && "$1" != "--" && "$1" != "" ]]; then
        # optional third positional before patterns: exe basename without .exe
        if [[ "$1" != "--" ]]; then
            exe_base="$1"
            shift
        fi
    fi
    # swallow optional --
    [[ $# -gt 0 && "$1" == "--" ]] && shift
    local patterns=("$@")

    local stage="${STAGE_ROOT}/${name}"
    local exe="${BUILD_DIR}/${exe_base}.exe"
    local log="${stage}/run.log"
    local rc=0

    [[ -f "$exe" ]] || { echo -e "  ${RED}FAIL${RESET} $name — missing $exe"; FAIL=$((FAIL+1)); return; }

    rm -rf "$stage"
    mkdir -p "$stage"

    # Stage under the prefix's drive_c so Wine sees a real Windows path
    # (C:\linuwux_test\...). The library's marker scan requires argv[1] to
    # look like X:\... — a bare Unix path fails the drive-letter check.
    local wineprefix="${stage}/prefix"
    local windir="${wineprefix}/drive_c/linuwux_test"
    mkdir -p "$windir"
    mkdir -p "${wineprefix}/dosdevices"
    # Minimal dosdevices so C: resolves without a full wineboot.
    ln -sfn ../drive_c "${wineprefix}/dosdevices/c:"
    ln -sfn / "${wineprefix}/dosdevices/z:"

    cp "$exe" "${windir}/${name}.exe"

    if [[ -n "$marker" ]]; then
        local src_marker="${MARKERS_DIR}/${marker}"
        [[ -f "$src_marker" ]] || die "Marker file missing: $src_marker"
        cp "$src_marker" "${windir}/${marker}"
        info "  staged $name + marker $marker (C:\\linuwux_test\\)"
    else
        info "  staged $name (no marker, C:\\linuwux_test\\)"
    fi

    set +e
    (
        export WINEPREFIX="$wineprefix"
        export WINEDEBUG="-all"
        export WINEDLLOVERRIDES="mscoree,mshtml="
        export LINUWUX_DEBUG=1
        export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}${LIB}"
        # Windows path so the constructor's argv[1] is C:\linuwux_test\name.exe
        "$WINE" "C:\\linuwux_test\\${name}.exe" 2>&1
    ) >"$log" 2>&1
    rc=$?
    set -e

    if [[ $VERBOSE -eq 1 ]]; then
        echo "----- $name log -----"
        cat "$log"
        echo "----- end -----"
    fi

    if [[ $rc -ne 0 ]]; then
        echo -e "  ${RED}FAIL${RESET} $name — PE exited $rc"
        [[ $VERBOSE -eq 0 ]] && { echo "  --- log ---"; tail -n 40 "$log"; }
        FAIL=$((FAIL + 1))
        return
    fi

    local missing=0
    local pat
    for pat in "${patterns[@]}"; do
        if ! grep -E -q -- "$pat" "$log"; then
            echo -e "  ${RED}missing pattern${RESET}: $pat"
            missing=1
        fi
    done

    if [[ $missing -eq 0 ]]; then
        echo -e "  ${GREEN}PASS${RESET} $name"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${RESET} $name — expected log lines missing"
        [[ $VERBOSE -eq 0 ]] && { echo "  --- log ---"; tail -n 60 "$log"; }
        FAIL=$((FAIL + 1))
    fi
}

# ------------------------------------------------------------
header "Running tests"
rm -rf "$STAGE_ROOT"
mkdir -p "$STAGE_ROOT"

# 1. Modern path — reflex marker, ARM only (no prior INIT)
stage_and_run modern_arm "reflex.dll" -- \
    '\[linuwux\] v[0-9].*loaded' \
    'Found reflex\.dll' \
    'cpuid arm leaf, protocol=modern' \
    'kuser_shared_data: patched \(modern\)'

# 2. Legacy single — reflex marker so the early DenuvOwO force does NOT fire.
#    INIT → LEGACY_SINGLE, ARM stores handler, KUSER applies legacy-single profile.
stage_and_run legacy_single "reflex.dll" -- \
    '\[linuwux\] v[0-9].*loaded' \
    'Found reflex\.dll' \
    'initialized legacy Reflex CPUID protocol' \
    'cpuid arm leaf, protocol=legacy' \
    'kuser_shared_data: patched \(legacy-single\)'

# 3. Legacy dual — same marker, full query sequence promotes to dual.
stage_and_run legacy_dual "reflex.dll" -- \
    '\[linuwux\] v[0-9].*loaded' \
    'Found reflex\.dll' \
    'initialized legacy Reflex CPUID protocol' \
    'cpuid arm leaf, protocol=legacy' \
    'kuser_shared_data: patched \(legacy-dual\)'

# 4. DenuvOwO-family marker still early-forces modern (current hybrid behaviour).
#    Reuses the modern_arm binary; only the marker changes.
stage_and_run denuvowo_force "DenuvOwO.ini" modern_arm -- \
    '\[linuwux\] v[0-9].*loaded' \
    'Found DenuvOwO\.ini' \
    'selected DenuvOwO SimpleSvm identity from marker' \
    'cpuid arm leaf, protocol=modern' \
    'kuser_shared_data: patched \(modern\)'

# 5. Control: no marker → must stay silent (no banner, no protocol activity).
# Handled separately so we assert absence rather than presence of patterns.
{
    name="no_marker"
    stage="${STAGE_ROOT}/${name}"
    exe="${BUILD_DIR}/${name}.exe"
    log="${stage}/run.log"
    wineprefix="${stage}/prefix"
    windir="${wineprefix}/drive_c/linuwux_test"

    rm -rf "$stage"
    mkdir -p "$windir"
    mkdir -p "${wineprefix}/dosdevices"
    ln -sfn ../drive_c "${wineprefix}/dosdevices/c:"
    ln -sfn / "${wineprefix}/dosdevices/z:"
    cp "$exe" "${windir}/${name}.exe"
    info "  staged $name (no marker, C:\\linuwux_test\\)"

    set +e
    (
        export WINEPREFIX="$wineprefix"
        export WINEDEBUG="-all"
        export WINEDLLOVERRIDES="mscoree,mshtml="
        export LINUWUX_DEBUG=1
        export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}${LIB}"
        "$WINE" "C:\\linuwux_test\\${name}.exe" 2>&1
    ) >"$log" 2>&1
    rc=$?
    set -e

    if [[ $VERBOSE -eq 1 ]]; then
        echo "----- $name log -----"
        cat "$log"
        echo "----- end -----"
    fi

    bad=0
    if [[ $rc -ne 0 ]]; then
        echo -e "  ${RED}FAIL${RESET} $name — PE exited $rc"
        bad=1
    fi
    if grep -E -q '\[linuwux\] v[0-9].*loaded' "$log"; then
        echo -e "  ${RED}FAIL${RESET} $name — version banner present (should be silent)"
        bad=1
    fi
    if grep -E -q 'cpuid arm leaf|kuser_shared_data: patched|selected DenuvOwO|initialized legacy' "$log"; then
        echo -e "  ${RED}FAIL${RESET} $name — protocol activity without marker"
        bad=1
    fi
    if [[ $bad -eq 0 ]]; then
        echo -e "  ${GREEN}PASS${RESET} $name"
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        [[ $VERBOSE -eq 0 ]] && { echo "  --- log ---"; tail -n 40 "$log"; }
    fi
}

# ------------------------------------------------------------
header "Results"
echo -e "  ${GREEN}PASS${RESET}: $PASS"
echo -e "  ${RED}FAIL${RESET}: $FAIL"
if [[ $KEEP_STAGE -eq 0 ]]; then
    rm -rf "$STAGE_ROOT"
    info "Stage dirs cleaned (pass --keep-stage to retain)"
else
    info "Stage dirs kept under $STAGE_ROOT"
fi

if [[ $FAIL -gt 0 ]]; then
    echo
    die "$FAIL test(s) failed"
fi

echo
info "All tests passed"
exit 0
