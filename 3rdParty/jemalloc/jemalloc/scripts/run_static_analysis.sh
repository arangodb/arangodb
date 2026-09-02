#!/usr/bin/env bash
set -euo pipefail

git clean -Xfd

export CC='clang'
export CXX='clang++'
compile_time_malloc_conf='background_thread:true,'\
'metadata_thp:auto,'\
'abort_conf:true,'\
'muzzy_decay_ms:0,'\
'zero_realloc:free,'\
'prof_unbias:false,'\
'prof_time_resolution:high'
extra_flags=(
	-Wmissing-prototypes
	-Wmissing-variable-declarations
	-Wstrict-prototypes
	-Wunreachable-code
	-Wunreachable-code-aggressive
	-Wunused-macros
)

EXTRA_CFLAGS="${extra_flags[*]}" EXTRA_CXXFLAGS="${extra_flags[*]}" ./autogen.sh \
	--with-private-namespace=jemalloc_ \
	--disable-cache-oblivious \
	--enable-prof \
	--enable-prof-libunwind \
	--with-malloc-conf="$compile_time_malloc_conf" \
	--enable-readlinkat \
	--enable-opt-safety-checks \
	--enable-uaf-detection \
	--enable-force-getenv \
	--enable-debug # Enabling debug for static analysis is important,
	               # otherwise you'll get tons of warnings for things
	               # that are already covered by `assert`s.

bear -- make -s -j "$(nproc)"
# We end up with lots of duplicate entries in the compilation database, one for
# each output file type (e.g. .o, .d, .sym, etc.). There must be exactly one
# entry for each file in the compilation database in order for
# cross-translation-unit analysis to work, so we deduplicate the database here.
jq '[.[] | select(.output | test("/[^./]*\\.o$"))]' compile_commands.json > compile_commands.json.tmp
mv compile_commands.json.tmp compile_commands.json

# CodeChecker has a bug where it freaks out if you supply the skipfile via process substitution,
# so we resort to manually creating a temporary file
skipfile=$(mktemp)
# The single-quotes are deliberate here, you want `$skipfile` to be evaluated upon exit
trap 'rm -f $skipfile' EXIT
echo '-**/stdlib.h' > "$skipfile"
CC_ANALYZERS_FROM_PATH=1 CodeChecker analyze compile_commands.json --jobs "$(nproc)" \
	--ctu --compile-uniqueing strict --output static_analysis_raw_results \
	--analyzers clangsa clang-tidy --skip "$skipfile" \
	--enable readability-inconsistent-declaration-parameter-name \
	--enable performance-no-int-to-ptr \
	--disable clang-diagnostic-reserved-macro-identifier
	# `--enable` is additive, the vast majority of the checks we want are
	# enabled by default.

html_output_dir="${1:-static_analysis_results}"
result=${2:-/dev/null}
# We're echoing a value because we want to indicate whether or not any errors
# were found, but we always want the script to have a successful exit code so
# that we actually reach the step in the GitHub action where we upload the results.
if CodeChecker parse --export html --output "$html_output_dir" static_analysis_raw_results
then
	echo "HAS_STATIC_ANALYSIS_RESULTS=0" >> "$result"
else
	echo "HAS_STATIC_ANALYSIS_RESULTS=1" >> "$result"
fi
