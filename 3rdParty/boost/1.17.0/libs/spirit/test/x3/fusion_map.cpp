/*=============================================================================
  Copyright (c) 2001-2015 Joel de Guzman

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
  =============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/make_map.hpp>
#include <boost/fusion/adapted/struct.hpp>

#include <string>
#include <iostream>
#include "test.hpp"

struct AdaptedStruct {
    std::string key1;
    std::string key2;
};

class key1_attr {};
class key2_attr {};

BOOST_FUSION_ADAPT_ASSOC_STRUCT(
    AdaptedStruct,
    (std::string, key1, class key1_attr)
    (std::string, key2, class key2_attr)
    )

template <class Parser, class Attribute>
bool test_attr(const std::string in,Parser const& p, Attribute& attr) {
    auto it = in.begin();
    return boost::spirit::x3::parse(it,in.end(), p, attr);
}

int
main()
{
    using spirit_test::test;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::attr;
    using boost::spirit::x3::char_;
    using boost::spirit::x3::eps;
    namespace fusion = boost::fusion;

    { // parsing sequence directly into fusion map

        auto const key1 = lit("key1") >> attr(key1_attr());
        auto const kv1 = key1 >> lit("=") >> +char_;

        {
           auto attr_ =  fusion::make_map<key1_attr>(std::string());
           BOOST_TEST(test_attr("key1=ABC", kv1, attr_));
           BOOST_TEST(fusion::at_key<key1_attr>(attr_) == "ABC");
        }
        {
           AdaptedStruct attr_;
           BOOST_TEST(test_attr("key1=ABC", kv1, attr_));
           BOOST_TEST(attr_.key1 == "ABC");
           BOOST_TEST(attr_.key2 == "");
        }
        }
        {   // case when parser handling fusion assoc sequence
            // is on one side of another sequence
            auto const key1 = lit("key1") >> attr(key1_attr());
            auto const kv1 = key1 >> lit("=") >> +~char_(';');

            AdaptedStruct attr_;
            BOOST_TEST(test_attr("key1=ABC", eps >>  (kv1 % ';') , attr_));
            BOOST_TEST(attr_.key1 == "ABC");
            BOOST_TEST(attr_.key2 == "");
        }
        { // parsing repeated sequence directly into fusion map (overwrite)
          auto const key1 = lit("key1") >> attr(key1_attr());
          auto const kv1 = key1 >> lit("=") >> +~char_(';');

        {
           auto attr_ =  fusion::make_map<key1_attr>(std::string());
           BOOST_TEST(test_attr("key1=ABC;key1=XYZ", kv1 % ';', attr_));
           BOOST_TEST(fusion::at_key<key1_attr>(attr_) == "XYZ");
        }
        {
           AdaptedStruct attr_;
           BOOST_TEST(test_attr("key1=ABC;key1=XYZ", kv1 % ';', attr_));
           BOOST_TEST(attr_.key1 == "XYZ");
        }
    }

    {   // parsing repeated sequence directly into fusion map (append)

        /* NOT IMPLEMENTED
        auto const key1 = lit("key1") >> attr(key1_attr());
        auto const kv1 = key1 >> lit("=") >> +char_;
        auto attr_ =  fusion::make_map<key1_attr>(std::vector<std::string>());

        BOOST_TEST(test_attr("key1=ABC;key1=XYZ", kv1 % ";", attr_));
        BOOST_TEST(fusion::at_key<key1_attr>(attr_) == {"ABC","XYZ"});
        */
    }

    { // alternative over key-value pairs

        auto const key1 = lit("key1") >> attr(key1_attr());
        auto const key2 = lit("key2") >> attr(key2_attr());
        auto const kv1 = key1 >> lit("=") >> +~char_(';');
        auto const kv2 = key2 >> lit("=") >> +~char_(';');

        auto attr_ =  fusion::make_map<key1_attr, key2_attr>(std::string(),std::string());
        BOOST_TEST(test_attr("key2=XYZ;key1=ABC", (kv1|kv2) % ';', attr_));
        BOOST_TEST(fusion::at_key<key1_attr>(attr_) == "ABC");
        BOOST_TEST(fusion::at_key<key2_attr>(attr_) == "XYZ");
    }

    { // parsing sequence where key is a variant

        namespace x3 = boost::spirit::x3;
        auto  key1 = lit("key1") >> attr(key1_attr());
        auto  key2 = lit("key2") >> attr(key2_attr());
        auto  keys = key1 | key2;
        auto pair = keys >> lit("=") >> +~char_(';');

        {
            auto attr_ =  fusion::make_map<key1_attr,key2_attr>(std::string(),std::string());
            BOOST_TEST(test_attr("key1=ABC;key2=XYZ", pair % ';', attr_));
            BOOST_TEST(fusion::at_key<key1_attr>(attr_) == "ABC");
            BOOST_TEST(fusion::at_key<key2_attr>(attr_) == "XYZ");
        }
        {
            AdaptedStruct attr_;
            BOOST_TEST(test_attr("key1=ABC;key2=XYZ", pair % ';', attr_));
            BOOST_TEST(fusion::at_key<key1_attr>(attr_) == "ABC");
            BOOST_TEST(fusion::at_key<key2_attr>(attr_) == "XYZ");
        }
    }

    return boost::report_errors();
}
