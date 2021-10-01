#!/bin/bash


# Transconding perf output
transcode_scale=2500000

if [ ! -f transcoding_perf.out ]; then
    echo "transcoding_perf"
    ../build/perf/transcoding_perf --benchmark_out_format=json --benchmark_out=transcoding_perf.out
fi

# UTF-8 to UTF-16 transcoding chart
./make_perf_chart.py transcoding_perf.out $transcode_scale BM_8_to_16_iterator_prealloc,'Iterators' BM_8_to_16_algorithm,'Algorithm std::back_inserter' BM_8_to_16_algorithm_prealloc_pointer,'Algorithm using SIMD' BM_8_to_16_algorithm_no_simd_prealloc,'Algorithm no SIMD' BM_8_to_16_algorithm_icu,ICU > utf_8_to_16_perf.svg

# UTF-8 to UTF-32 transcoding chart
./make_perf_chart.py transcoding_perf.out $transcode_scale BM_8_to_32_iterator_prealloc,'Iterators' BM_8_to_32_algorithm,'Algorithm std::back_inserter' BM_8_to_32_algorithm_prealloc_pointer,'Algorithm using SIMD' BM_8_to_32_algorithm_no_simd_prealloc,'Algorithm no SIMD' > utf_8_to_32_perf.svg




# Normalization perf output (from NFC/European languages)
nfc_norm_scale=2500000
nfd_norm_scale=5000000

if [ ! -f icu_normalization_euro_from_nfc.out ]; then
    echo "icu_normalization --european --from-nfc"
    ../build/perf/icu_normalization --european --from-nfc --benchmark_out_format=json --benchmark_out=icu_normalization_euro_from_nfc.out
fi

# NFC normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfc.out $nfc_norm_scale BM_text_utf8_nfc,'Algorithm with back-inserter' BM_text_utf8_nfc_string_append,'String append' BM_icu_utf8_nfc,'ICU' BM_icu_utf16_nfc,'ICU UTF-16'> norm_nfc_euro_from_nfc_perf.svg

# FCC normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfc.out $nfc_norm_scale BM_text_utf8_fcc,'Algorithm with back-inserter' BM_text_utf8_fcc_string_append,'String append' BM_icu_utf8_fcc,'ICU' BM_icu_utf16_fcc,'ICU UTF-16'> norm_fcc_euro_from_nfc_perf.svg

# NFD normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfc.out $nfd_norm_scale BM_text_utf8_nfd,'Algorithm with back-inserter' BM_text_utf8_nfd_string_append,'String append' BM_icu_utf8_nfd,'ICU' BM_icu_utf16_nfd,'ICU UTF-16'> norm_nfd_euro_from_nfc_perf.svg


# Normalization perf output (from NFC/Non-European languages)
if [ ! -f icu_normalization_non_euro_from_nfc.out ]; then
    echo "icu_normalization --non-european --from-nfc"
    ../build/perf/icu_normalization --non-european --from-nfc --benchmark_out_format=json --benchmark_out=icu_normalization_non_euro_from_nfc.out
fi

# NFC normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfc.out $nfc_norm_scale BM_text_utf8_nfc,'Algorithm with back-inserter' BM_text_utf8_nfc_string_append,'String append' BM_icu_utf8_nfc,'ICU' BM_icu_utf16_nfc,'ICU UTF-16'> norm_nfc_non_euro_from_nfc_perf.svg

# FCC normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfc.out $nfc_norm_scale BM_text_utf8_fcc,'Algorithm with back-inserter' BM_text_utf8_fcc_string_append,'String append' BM_icu_utf8_fcc,'ICU' BM_icu_utf16_fcc,'ICU UTF-16'> norm_fcc_non_euro_from_nfc_perf.svg

# NFD normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfc.out $nfd_norm_scale BM_text_utf8_nfd,'Algorithm with back-inserter' BM_text_utf8_nfd_string_append,'String append' BM_icu_utf8_nfd,'ICU' BM_icu_utf16_nfd,'ICU UTF-16'> norm_nfd_non_euro_from_nfc_perf.svg


# Normalization perf output (from NFD/European languages)
if [ ! -f icu_normalization_euro_from_nfd.out ]; then
    echo "icu_normalization --european --from-nfd"
    ../build/perf/icu_normalization --european --from-nfd --benchmark_out_format=json --benchmark_out=icu_normalization_euro_from_nfd.out
fi

# NFC normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfd.out $nfc_norm_scale BM_text_utf8_nfc,'Algorithm with back-inserter' BM_text_utf8_nfc_string_append,'String append' BM_icu_utf8_nfc,'ICU' BM_icu_utf16_nfc,'ICU UTF-16'> norm_nfc_euro_from_nfd_perf.svg

# FCC normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfd.out $nfc_norm_scale BM_text_utf8_fcc,'Algorithm with back-inserter' BM_text_utf8_fcc_string_append,'String append' BM_icu_utf8_fcc,'ICU' BM_icu_utf16_fcc,'ICU UTF-16'> norm_fcc_euro_from_nfd_perf.svg

# NFD normalization chart
./make_perf_chart.py icu_normalization_euro_from_nfd.out $nfd_norm_scale BM_text_utf8_nfd,'Algorithm with back-inserter' BM_text_utf8_nfd_string_append,'String append' BM_icu_utf8_nfd,'ICU' BM_icu_utf16_nfd,'ICU UTF-16'> norm_nfd_euro_from_nfd_perf.svg


# Normalization perf output (from NFD/Non-European languages)
if [ ! -f icu_normalization_non_euro_from_nfd.out ]; then
    echo "icu_normalization --non-european --from-nfd"
    ../build/perf/icu_normalization --non-european --from-nfd --benchmark_out_format=json --benchmark_out=icu_normalization_non_euro_from_nfd.out
fi

# NFC normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfd.out $nfc_norm_scale BM_text_utf8_nfc,'Algorithm with back-inserter' BM_text_utf8_nfc_string_append,'String append' BM_icu_utf8_nfc,'ICU' BM_icu_utf16_nfc,'ICU UTF-16'> norm_nfc_non_euro_from_nfd_perf.svg

# FCC normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfd.out $nfc_norm_scale BM_text_utf8_fcc,'Algorithm with back-inserter' BM_text_utf8_fcc_string_append,'String append' BM_icu_utf8_fcc,'ICU' BM_icu_utf16_fcc,'ICU UTF-16'> norm_fcc_non_euro_from_nfd_perf.svg

# NFD normalization chart
./make_perf_chart.py icu_normalization_non_euro_from_nfd.out $nfd_norm_scale BM_text_utf8_nfd,'Algorithm with back-inserter' BM_text_utf8_nfd_string_append,'String append' BM_icu_utf8_nfd,'ICU' BM_icu_utf16_nfd,'ICU UTF-16'> norm_nfd_non_euro_from_nfd_perf.svg
