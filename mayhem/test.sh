#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the known-answer suite for the data_handler library (built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

PROBE="$SRC/build/test/kat_probe"
if [ ! -x "$PROBE" ]; then
  echo "missing $PROBE — mayhem/build.sh did not produce it" >&2
  emit_ctrf "brainflow-kat" 0 1 0
  exit 1
fi

# Expected output of the probe (known inputs -> analytically known results: FFT of 1..8, Haar wavelet of
# 1..8, Hann/Hamming windows, PSD peak of a 32 Hz sine, filter DC gain, file round trip, error codes).
read -r -d '' EXPECTED <<'EOF'
version: 0 0.0.1
pow2: 0 1024
pow2_zero: 13
fft_res: 0
fft_re: 36.0000 -4.0000 -4.0000 -4.0000 -4.0000
fft_im: 0.0000 9.6569 4.0000 1.6569 0.0000
ifft_res: 0
ifft: 1.0000 2.0000 3.0000 4.0000 5.0000 6.0000 7.0000 8.0000
fft_odd_len: 13
fft_null: 13
hann_res: 0
hann: 0.0000 0.1464 0.5000 0.8536 1.0000 0.8536 0.5000 0.1464
hamming: 0.0800 0.2147 0.5400 0.8653 1.0000 0.8653 0.5400 0.2147
window_bad: 13
detrend_const_res: 0
detrend_const: -2.0000 -1.0000 0.0000 1.0000 2.0000
detrend_linear: 0.0000 0.0000 0.0000 0.0000 0.0000 0.0000
down_mean_res: 0
down_mean: 1.5000 3.5000 5.5000
down_median: 2.0000 5.0000
rolling_res: 0
rolling_mean: 1.0000 1.5000 2.0000 3.0000 4.0000 5.0000
decim_res: 0
decim_len4_finite: 1.0000
stddev: 0 2.5820
stddev_bad: 13
lowpass_res: 0
lowpass_dc: 5.000
highpass_dc: 0.000
lowpass_attenuates: 1
lowpass_bad_cutoff: 13
bandpass_passes: 1
psd_res: 0
psd_peak_freq: 32.000
band_power_dominant: 1
welch_res: 0
welch_peak_freq: 32.000
haar_res: 0 lens 4 4
haar_coeffs: 2.1213 4.9497 7.7782 10.6066 -0.7071 -0.7071 -0.7071 -0.7071
haar_inv_res: 0
haar_inv: 1.0000 2.0000 3.0000 4.0000 5.0000 6.0000 7.0000 8.0000
write_res: 0
count: 0 6
read_res: 0 rows 2 cols 3
read_back: 1.5000 -2.0000 3.2500 4.0000 5.5000 -6.7500
read_short: 9
read_missing: 13
flatline: 0 100.00
clipping: 0 22.00
EOF

work=$(mktemp -d)
actual=$(cd "$work" && "$PROBE" 2>/dev/null)
rm -rf "$work"

passed=0 failed=0
while IFS= read -r want; do
  [ -n "$want" ] || continue
  if printf '%s\n' "$actual" | grep -qxF -- "$want"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL: expected line: $want" >&2
  fi
done <<< "$EXPECTED"

# Any extra probe output beyond the expected lines is also a failure (output must match exactly).
extra=$(diff <(printf '%s\n' "$EXPECTED") <(printf '%s\n' "$actual") | grep -c '^>' || true)
[ "$extra" -eq 0 ] || { failed=$((failed + extra)); echo "FAIL: $extra unexpected probe output lines" >&2; }

emit_ctrf "brainflow-kat" "$passed" "$failed" 0
