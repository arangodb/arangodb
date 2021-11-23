/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   security_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for security.hpp
 */

#include <boost/winapi/security.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_APP_SYSTEM

    BOOST_WINAPI_TEST_TYPE_SAME(PSID);
    BOOST_WINAPI_TEST_TYPE_SAME(SECURITY_DESCRIPTOR_CONTROL);
    BOOST_WINAPI_TEST_TYPE_SAME(PSECURITY_DESCRIPTOR_CONTROL);

    BOOST_WINAPI_TEST_STRUCT(ACL, (AclRevision)(Sbz1)(AclSize)(AceCount)(Sbz2));
    BOOST_WINAPI_TEST_STRUCT(SECURITY_DESCRIPTOR, (Revision)(Sbz1)(Control)(Owner)(Group)(Sacl)(Dacl));

#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM

    return boost::report_errors();
}
