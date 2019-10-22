// -----------------------------------------------------------
//              Copyright (c) 2001 Jeremy Siek
//           Copyright (c) 2003-2006 Gennaro Prota
//
// Copyright (c) 2015 Seth Heeren
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#include "boost/config.hpp"
#if !defined (BOOST_NO_STRINGSTREAM)
# include <sstream>
#endif

#include "bitset_test.hpp"
#include <boost/dynamic_bitset/serialization.hpp>
#include <boost/config/workaround.hpp>


// Codewarrior 8.3 for Win fails without this.
// Thanks Howard Hinnant ;)
#if defined __MWERKS__ && BOOST_WORKAROUND(__MWERKS__, <= 0x3003) // 8.x
# pragma parse_func_templ off
#endif


#if defined BOOST_NO_STD_WSTRING || defined BOOST_NO_STD_LOCALE
# define BOOST_DYNAMIC_BITSET_NO_WCHAR_T_TESTS
#endif

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <sstream>

namespace {
    template <typename Block>
        struct SerializableType {
            boost::dynamic_bitset<Block> x;

          private:
            friend class boost::serialization::access;
            template <class Archive> void serialize(Archive &ar, const unsigned int) {
                ar & BOOST_SERIALIZATION_NVP(x);
            }
        };

    template <typename Block, typename IArchive, typename OArchive>
        void test_serialization( BOOST_EXPLICIT_TEMPLATE_TYPE(Block) )
        {
            SerializableType<Block> a;

            for (int i=0; i<128; ++i)
                a.x.resize(11*i, i%2);

#if !defined (BOOST_NO_STRINGSTREAM)
            std::stringstream ss;

            // test serialization
            {
                OArchive oa(ss);
                oa << BOOST_SERIALIZATION_NVP(a);
            }

            // test de-serialization
            {
                IArchive ia(ss);
                SerializableType<Block> b;
                ia >> BOOST_SERIALIZATION_NVP(b);

                assert(a.x == b.x);
            }
#else
#           error "TODO implement file-based test path?"
#endif
        }

    template <typename Block>
        void test_binary_archive( BOOST_EXPLICIT_TEMPLATE_TYPE(Block) ) {
            test_serialization<Block, boost::archive::binary_iarchive, boost::archive::binary_oarchive>();
        }

    template <typename Block>
        void test_xml_archive( BOOST_EXPLICIT_TEMPLATE_TYPE(Block) ) {
            test_serialization<Block, boost::archive::xml_iarchive, boost::archive::xml_oarchive>();
        }
}

template <typename Block>
void run_test_cases( BOOST_EXPLICIT_TEMPLATE_TYPE(Block) )
{
    test_binary_archive<Block>();
    test_xml_archive<Block>();
}

int main()
{
    run_test_cases<unsigned char>();
    run_test_cases<unsigned short>();
    run_test_cases<unsigned int>();
    run_test_cases<unsigned long>();
# ifdef BOOST_HAS_LONG_LONG
    run_test_cases< ::boost::ulong_long_type>();
# endif

    return boost::report_errors();
}
