# Copyright 2019 Hans Dembinski
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

# keep in sync with Jamfile
boost_test(TYPE compile-fail SOURCES make_histogram_fail0.cpp)
boost_test(TYPE compile-fail SOURCES make_histogram_fail1.cpp)
boost_test(TYPE run SOURCES algorithm_project_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES algorithm_reduce_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES algorithm_sum_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_category_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_integer_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_option_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_regular_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_size.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_traits_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_variable_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES axis_variant_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_args_type_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_axes_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_convert_integer_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_compressed_pair_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_detect_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_limits_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_make_default_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_misc_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_meta_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_large_int_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_linearize_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_iterator_adaptor_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_relaxed_equal_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_replace_default_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_safe_comparison_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES detail_tuple_slice_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES histogram_dynamic_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES histogram_growing_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES histogram_mixed_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES histogram_operators_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES histogram_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES indexed_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES internal_accumulators_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES storage_adaptor_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES unlimited_storage_test.cpp
  LIBRARIES Boost::histogram Boost::core)
boost_test(TYPE run SOURCES utility_test.cpp
  LIBRARIES Boost::histogram Boost::core)

if (cxx_std_17 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  boost_test(TYPE run SOURCES deduction_guides_test.cpp
    LIBRARIES Boost::histogram Boost::core)
  target_compile_features(BoostHistogram-deduction_guides_test_cpp PRIVATE cxx_std_17)
endif()

if (Threads_FOUND)
  boost_test(TYPE run SOURCES histogram_threaded_test.cpp
    LIBRARIES Boost::histogram Boost::core Threads::Threads)
  boost_test(TYPE run SOURCES storage_adaptor_threaded_test.cpp
    LIBRARIES Boost::histogram Boost::core Threads::Threads)
endif()

## No cmake support yet
# boost_test(TYPE run SOURCES boost_accumulators_support_test.cpp
#  LIBRARIES Boost::histogram Boost::core Boost::accumulators)
# boost_test(TYPE run SOURCES boost_range_support_test.cpp
#  LIBRARIES Boost::histogram Boost::core Boost::range)
# boost_test(TYPE run SOURCES boost_units_support_test.cpp
#  LIBRARIES Boost::histogram Boost::core Boost::units)
# boost_test(TYPE run SOURCES unlimited_storage_serialization_test.cpp LIBRARIES Boost::histogram Boost::core Boost::serialization)
# boost_test(TYPE run SOURCES storage_adaptor_serialization_test.cpp LIBRARIES Boost::histogram Boost::core Boost::serialization)
# boost_test(TYPE run SOURCES histogram_serialization_test.cpp LIBRARIES Boost::histogram Boost::core Boost::serialization)
# boost_test(TYPE run SOURCES axis_variant_serialization_test.cpp
#   LIBRARIES Boost::histogram Boost::core Boost::serialization)

boost_test(SOURCES check_cmake_version.cpp ARGUMENTS ${PROJECT_VERSION} LIBRARIES Boost::core Boost::config)
