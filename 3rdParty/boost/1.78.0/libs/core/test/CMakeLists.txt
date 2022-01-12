# Copyright 2018, 2019, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

boost_test_jamfile(FILE Jamfile.v2 LINK_LIBRARIES Boost::core Boost::static_assert)

set(BOOST_TEST_LINK_LIBRARIES Boost::core Boost::type_traits)

boost_test(TYPE run SOURCES eif_constructors.cpp)
boost_test(TYPE run SOURCES eif_dummy_arg_disambiguation.cpp)
boost_test(TYPE run SOURCES eif_lazy.cpp)
boost_test(TYPE run SOURCES eif_lazy_test.cpp)
boost_test(TYPE run SOURCES eif_member_templates.cpp)
boost_test(TYPE run SOURCES eif_namespace_disambiguation.cpp)
boost_test(TYPE run SOURCES eif_no_disambiguation.cpp)
boost_test(TYPE run SOURCES eif_partial_specializations.cpp)

set(BOOST_TEST_LINK_LIBRARIES Boost::core Boost::throw_exception)

boost_test(TYPE run SOURCES no_exceptions_support_test.cpp)

endif()

add_subdirectory(swap)
