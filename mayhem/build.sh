#!/usr/bin/env bash
#
# mayhem/build.sh — builds, from source and offline:
#   * the data_handler library (BrainFlow's signal-processing core) and its third-party static
#     libs, sanitized + DWARF-3 + coverage-instrumented, linked into one fuzz binary and one
#     -standalone reproducer per harness, and
#   * a normal-flags (unsanitized) libDataHandler.so plus the known-answer probe that mayhem/test.sh runs.
# The upstream CMake build is not used: its DataHandler target is SHARED and copies the library into
# every language package dir; compiling the same sources directly gives identical code without that.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

TP="$SRC/third_party"
VERSION=$(sed -n 's/^SET (BRAINFLOW_VERSION "\([^"]*\)".*/\1/p' CMakeLists.txt)
INCS="-I$TP -I$SRC/src/utils/inc -I$SRC/src/data_handler/inc -I$TP/DSPFilters/include \
-I$TP/wavelib/header -I$TP/wavelib/src -I$TP/kissfft"
DH_SRC="data_handler signal_processing signal_metrics decomposition fastica"

# compile_lib <objdir> <c flags> <c++ flags>: every third-party and data_handler source -> <objdir>/*.o
compile_lib() {
    local out="$1" cflags="$2" cxxflags="$3"
    rm -rf "$out" && mkdir -p "$out"
    {
        for f in kiss_fft kfc kiss_fftnd kiss_fftndr kiss_fftr; do
            echo "$CC $cflags -Dkiss_fft_scalar=double $INCS -c $TP/kissfft/$f.c -o $out/kissfft_$f.o"
        done
        for f in "$TP"/wavelib/src/*.cpp; do
            echo "$CXX $cxxflags $INCS -c $f -o $out/wavelib_$(basename "$f" .cpp).o"
        done
        for f in "$TP"/DSPFilters/source/*.cpp; do
            echo "$CXX $cxxflags -DNDEBUG $INCS -c $f -o $out/dsp_$(basename "$f" .cpp).o"
        done
        for f in $DH_SRC; do
            echo "$CXX $cxxflags -DBRAINFLOW_VERSION=$VERSION $INCS -c $SRC/src/data_handler/$f.cpp -o $out/dh_$f.o"
        done
    } | xargs -P "$MAYHEM_JOBS" -I{} bash -c '{}'
}

# 1) sanitized + coverage-instrumented library objects (fuzz and standalone binaries)
FUZZ_C="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1"
FUZZ_CXX="$FUZZ_C -std=c++11"
compile_lib "$SRC/build/fuzz" "$FUZZ_C" "$FUZZ_CXX"

# 2) lsan_off hook + standalone driver as C, then each harness twice
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$SRC/build/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$SRC/build/standalone_main.o"
for t in fuzz_read_file fuzz_filters fuzz_spectral fuzz_decomposition; do
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 -std=c++17 $INCS \
        -I"$SRC/mayhem" -c "$SRC/mayhem/$t.cc" -o "$SRC/build/$t.o"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$SRC/build/$t.o" "$SRC"/build/fuzz/*.o \
        "$SRC/build/lsan_off.o" -lpthread -ldl -lm -o "/mayhem/$t"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$SRC/build/$t.o" "$SRC/build/standalone_main.o" \
        "$SRC"/build/fuzz/*.o "$SRC/build/lsan_off.o" -lpthread -ldl -lm -o "/mayhem/$t-standalone"
done

# 3) normal-flags shared library + known-answer probe for mayhem/test.sh (clean, unsanitized build)
NORM_C="-O2 -fPIC -fvisibility=hidden $COVERAGE_FLAGS"
compile_lib "$SRC/build/normal" "$NORM_C" "$NORM_C -std=c++11"
mkdir -p "$SRC/build/test"
$CXX -shared $COVERAGE_FLAGS "$SRC"/build/normal/*.o -lpthread -ldl -lm -o "$SRC/build/test/libDataHandler.so"
$CXX -O2 -std=c++17 $COVERAGE_FLAGS -I"$SRC/src/utils/inc" -I"$SRC/src/data_handler/inc" \
    "$SRC/mayhem/kat_probe.cc" -L"$SRC/build/test" -lDataHandler -Wl,-rpath,"$SRC/build/test" \
    -o "$SRC/build/test/kat_probe"
