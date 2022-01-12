//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/atomic/atomic_ref.hpp>
#include <boost/atomic/capabilities.hpp>

#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#include "wait_test_helpers.hpp"

int main(int, char *[])
{
    test_wait_notify_api< atomic_ref_wrapper, boost::uint8_t >(1, 2, 3, BOOST_ATOMIC_HAS_NATIVE_INT8_WAIT_NOTIFY);
    test_wait_notify_api< atomic_ref_wrapper, boost::uint16_t >(1, 2, 3, BOOST_ATOMIC_HAS_NATIVE_INT16_WAIT_NOTIFY);
    test_wait_notify_api< atomic_ref_wrapper, boost::uint32_t >(1, 2, 3, BOOST_ATOMIC_HAS_NATIVE_INT32_WAIT_NOTIFY);
    test_wait_notify_api< atomic_ref_wrapper, boost::uint64_t >(1, 2, 3, BOOST_ATOMIC_HAS_NATIVE_INT64_WAIT_NOTIFY);
#if defined(BOOST_HAS_INT128) && !defined(BOOST_ATOMIC_TESTS_NO_INT128)
    test_wait_notify_api< atomic_ref_wrapper, boost::uint128_type >(1, 2, 3, BOOST_ATOMIC_HAS_NATIVE_INT128_WAIT_NOTIFY);
#endif

    {
        struct_3_bytes s1 = {{ 1 }};
        struct_3_bytes s2 = {{ 2 }};
        struct_3_bytes s3 = {{ 3 }};
        test_wait_notify_api< atomic_ref_wrapper, struct_3_bytes >(s1, s2, s3);
    }
    {
        large_struct s1 = {{ 1 }};
        large_struct s2 = {{ 2 }};
        large_struct s3 = {{ 3 }};
        test_wait_notify_api< atomic_ref_wrapper, large_struct >(s1, s2, s3);
    }

    return boost::report_errors();
}
