# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

boost_test_jamfile(FILE Jamfile.v2 LINK_LIBRARIES Boost::throw_exception Boost::core Boost::exception)

endif()

#run throw_from_library_test.cpp lib1_throw lib2_throw lib3_throw : : : <link>static : throw_from_library_static ;
#run throw_from_library_test.cpp lib1_throw lib2_throw lib3_throw : : : <link>shared : throw_from_library_shared ;

#run throw_exception_nx_test.cpp : : : <exception-handling>off ;
#run throw_exception_nx_test2.cpp : : : <exception-handling>off ;
