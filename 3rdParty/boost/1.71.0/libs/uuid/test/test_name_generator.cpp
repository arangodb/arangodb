//  (C) Copyright Andy Tompkins 2010. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

//  libs/uuid/test/test_name_generator.cpp  -------------------------------//

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/name_generator_md5.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/predef/library/c/cloudabi.h>

int main(int, char*[])
{
    using namespace boost::uuids;

    // Verify well-known uuid namespaces
    BOOST_TEST_EQ(ns::dns(),
                  string_generator()("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ(ns::url(),
                  string_generator()("6ba7b811-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ(ns::oid(),
                  string_generator()("6ba7b812-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ(ns::x500dn(),
                  string_generator()("6ba7b814-9dad-11d1-80b4-00c04fd430c8"));

    uuid correct_sha1  = {{0x21, 0xf7, 0xf8, 0xde, 0x80, 0x51, 0x5b, 0x89, 0x86, 0x80, 0x01, 0x95, 0xef, 0x79, 0x8b, 0x6a}};
    uuid wcorrect_sha1 = {{0xc3, 0x15, 0x27, 0x0b, 0xa4, 0x66, 0x58, 0x72, 0xac, 0xa4, 0x96, 0x26, 0xce, 0xc0, 0xf4, 0xbe}};
#if !defined(BOOST_UUID_COMPAT_PRE_1_71_MD5)
    // this value is stable across all endian systems
    uuid correct_md5   = {{0x06, 0x20, 0x5c, 0xec, 0x25, 0x5b, 0x30, 0x0e, 0xa8, 0xbc, 0xa8, 0x60, 0x5a, 0xb8, 0x24, 0x4e}};
#else
    // code before 1.71 had different results depending on the endianness
# if BOOST_ENDIAN_LITTLE_BYTE
    uuid correct_md5   = {{0xec, 0x5c, 0x20, 0x06, 0x0e, 0x00, 0x3b, 0x25, 0xa0, 0xa8, 0xbc, 0xe8, 0x4e, 0x24, 0xb8, 0x5a}};
# else
    uuid correct_md5   = {{0x06, 0x20, 0x5c, 0xec, 0x25, 0x5b, 0x30, 0x0e, 0xa8, 0xbc, 0xa8, 0x60, 0x5a, 0xb8, 0x24, 0x4e}};
# endif
#endif


    name_generator_sha1 gen(ns::dns());

    uuid u = gen("www.widgets.com");
    BOOST_TEST_EQ(u, correct_sha1);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);
    BOOST_TEST_EQ(u.version(), boost::uuids::uuid::version_name_based_sha1);

    // RFC 4122 Section 4.3 Bullet 1, same name in same namespace makes the same UUID
    u = gen(std::string("www.widgets.com"));
    BOOST_TEST_EQ(u, correct_sha1);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);

    // RFC 4122 Section 4.3 Bullet 2, two names in the same namespace makes a different UUID
    uuid u2 = gen("www.wonka.com");
    BOOST_TEST_NE(u, u2);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);

    u = gen(L"www.widgets.com");
    BOOST_TEST_EQ(u, wcorrect_sha1);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);

#ifndef BOOST_NO_STD_WSTRING
    u = gen(std::wstring(L"www.widgets.com"));
    BOOST_TEST_EQ(u, wcorrect_sha1);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);
#endif

    char name[] = "www.widgets.com";
    u = gen(name, 15);
    BOOST_TEST_EQ(u, correct_sha1);
    BOOST_TEST_EQ(u.variant(), boost::uuids::uuid::variant_rfc_4122);

    // RFC 4122 Section 4.3 Bullet 3, same name in different namespaces makes a different UUID
    name_generator_sha1 other(ns::url());
    uuid u3 = other("www.widgets.com");
    BOOST_TEST_NE(u, u3);

#if !BOOST_LIB_C_CLOUDABI
    // used by documentation
    uuid udoc = gen("boost.org");
    std::cout << "boost.org uuid in dns namespace: " << udoc << std::endl;
    // boost.org uuid in dns namespace: 0043f363-bbb4-5369-840a-322df6ec1926
#endif

    // deprecated equality check, make sure boost::uuids::name_generator is a sha1 generator
    name_generator other2(ns::url());
    uuid u4 = other2("www.widgets.com");
    BOOST_TEST_EQ(u3, u4);

    // MD5 generator
    name_generator_md5 mdgen(ns::url());
    uuid md = mdgen("www.widgets.com");
    BOOST_TEST_NE(u3, md);
    BOOST_TEST_EQ(md.variant(), boost::uuids::uuid::variant_rfc_4122);
    BOOST_TEST_EQ(md.version(), boost::uuids::uuid::version_name_based_md5);
    BOOST_TEST_EQ(md, correct_md5);

    // latest generator is SHA1 (test will break on change, which is good)
    name_generator_latest latestgen(ns::dns());
    uuid latest = latestgen("www.widgets.com");
    BOOST_TEST_EQ(latest, correct_sha1);

    return boost::report_errors();
}
