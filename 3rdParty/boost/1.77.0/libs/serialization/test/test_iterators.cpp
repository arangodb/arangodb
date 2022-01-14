/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_iterators.cpp

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <vector>
#include <cstdlib> // for rand
#include <functional>
#include <sstream> // used to test stream iterators
#include <clocale>
#include <iterator> // begin
#include <locale> // setlocale

#include <boost/config.hpp>
#ifdef BOOST_NO_STDC_NAMESPACE
namespace std{
    using ::rand;
}
#endif

#include <boost/detail/workaround.hpp>
#if BOOST_WORKAROUND(BOOST_DINKUMWARE_STDLIB, == 1)
#include <boost/archive/dinkumware.hpp>
#endif

#ifndef BOOST_NO_CWCHAR
#include <boost/archive/iterators/mb_from_wchar.hpp>
#include <boost/archive/iterators/wchar_from_mb.hpp>
#endif
#include <boost/archive/iterators/xml_escape.hpp>
#include <boost/archive/iterators/xml_unescape.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/istream_iterator.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include "test_tools.hpp"

#ifndef BOOST_NO_CWCHAR

void test_wchar_from_mb(const wchar_t *la, const char * a, const unsigned int size){
    typedef boost::archive::iterators::wchar_from_mb<const char *> translator;
    BOOST_CHECK((
        std::equal(
            translator(a),
            translator(a + size),
            la
        )
    ));
}

void test_mb_from_wchar(const char * a, const wchar_t *la, const unsigned int size){
    typedef boost::archive::iterators::mb_from_wchar<const wchar_t *> translator;
    BOOST_CHECK(
        std::equal(
            translator(la),
            translator(la + size),
            a
        )
    );
}

void test_roundtrip(const wchar_t * la){
    std::size_t s = std::wcslen(la);
    std::vector<char> a;
    {
        typedef boost::archive::iterators::mb_from_wchar<const wchar_t *> translator;
        std::copy(
            translator(la),
            translator(la + s),
            std::back_inserter(a)
        );
        // note: wchar_from_mb NEEDS a termination null in order to function!
        a.push_back(static_cast<char>(0));
    }
    BOOST_CHECK(a.size() > 0);
    std::vector<wchar_t> la2;
    {
        typedef boost::archive::iterators::wchar_from_mb<std::vector<char>::const_iterator> translator;
        std::copy(
            translator(a.begin()),
            translator(),
            std::back_inserter(la2)
        );
    }
    BOOST_CHECK(la2.size() == s);
    BOOST_CHECK(std::equal(la, la + s, la2.begin()));
}
#endif

template<class CharType>
void test_xml_escape(
    const CharType * xml_escaped,
    const CharType * xml,
    unsigned int size
){
    typedef boost::archive::iterators::xml_escape<const CharType *> translator;

    BOOST_CHECK(
        std::equal(
            translator(xml),
            translator(xml + size),
            xml_escaped
        )
    );
}

template<class CharType>
void test_xml_unescape(
    const CharType * xml,
    const CharType * xml_escaped,
    unsigned int size
){

    // test xml_unescape
    typedef boost::archive::iterators::xml_unescape<const CharType *> translator;

    BOOST_CHECK(
        std::equal(
            translator(xml_escaped),
            translator(xml_escaped + size),
            xml
        )
    );
}

template<int BitsOut, int BitsIn>
void test_transform_width(unsigned int size){
    // test transform_width
    char rawdata[8];

    char * rptr;
    for(rptr = rawdata + size; rptr-- > rawdata;)
        *rptr = static_cast<char>(0xff & std::rand());

    // convert 8 to 6 bit characters
    typedef boost::archive::iterators::transform_width<
        char *, BitsOut, BitsIn
    > translator1;

    std::vector<char> vout;

    std::copy(
        translator1(static_cast<char *>(rawdata)),
        translator1(rawdata + size),
        std::back_inserter(vout)
    );

    // check to see we got the expected # of characters out
    if(0 ==  size)
        BOOST_CHECK(vout.size() == 0);
    else
        BOOST_CHECK(vout.size() == (size * BitsIn - 1 ) / BitsOut + 1);

    typedef boost::archive::iterators::transform_width<
        std::vector<char>::iterator, BitsIn, BitsOut
    > translator2;

    std::vector<char> vin;
    std::copy(
        translator2(vout.begin()),
        translator2(vout.end()),
        std::back_inserter(vin)
    );

    // check to see we got the expected # of characters out
    BOOST_CHECK(vin.size() == size);

    BOOST_CHECK(
        std::equal(
            rawdata,
            rawdata + size,
            vin.begin()
        )
    );
}

template<class CharType>
void test_stream_iterators(
    const CharType * test_data,
    unsigned int size
){
    std::basic_stringstream<CharType> ss;
    boost::archive::iterators::ostream_iterator<CharType> osi =
        boost::archive::iterators::ostream_iterator<CharType>(ss);
    std::copy(test_data, test_data + size, osi);

    BOOST_CHECK(size == ss.str().size());

    boost::archive::iterators::istream_iterator<CharType> isi =
        boost::archive::iterators::istream_iterator<CharType>(ss);
    BOOST_CHECK(std::equal(test_data, test_data + size,isi));
}

int
test_main(int /* argc */, char* /* argv */ [] )
{
    const char xml[] = "<+>+&+\"+'";
    const char xml_escaped[] = "&lt;+&gt;+&amp;+&quot;+&apos;";
    test_xml_escape<const char>(
        xml_escaped,
        xml,
        sizeof(xml) / sizeof(char) - 1
    );
    test_xml_unescape<const char>(
        xml,
        xml_escaped,
        sizeof(xml_escaped) / sizeof(char) - 1
    );


    #ifndef BOOST_NO_CWCHAR
    const wchar_t wxml[] = L"<+>+&+\"+'";
    const wchar_t wxml_escaped[] = L"&lt;+&gt;+&amp;+&quot;+&apos;";
    test_xml_escape<const wchar_t>(
        wxml_escaped,
        wxml,
        sizeof(wxml) / sizeof(wchar_t) - 1
    );
    test_xml_unescape<const wchar_t>(
        wxml,
        wxml_escaped,
        sizeof(wxml_escaped) / sizeof(wchar_t) - 1
    );

    const char b[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    const wchar_t lb[] = L"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    test_mb_from_wchar(b, lb, sizeof(lb) / sizeof(wchar_t) - 1);
    test_wchar_from_mb(lb, b, sizeof(b) / sizeof(char) - 1);

    const char a[] = "abcdefghijklmnopqrstuvwxyz";
    const wchar_t la[] = L"abcdefghijklmnopqrstuvwxyz";
    test_mb_from_wchar(a, la, sizeof(la) / sizeof(wchar_t) - 1);
    test_wchar_from_mb(la, a, sizeof(a) / sizeof(char) - 1);

    test_roundtrip(L"z\u00df\u6c34\U0001f34c");

    test_stream_iterators<wchar_t>(la, sizeof(la)/sizeof(wchar_t) - 1);
    #endif

    test_stream_iterators<char>(a, sizeof(a) - 1);

    test_transform_width<6, 8>(0);
    test_transform_width<6, 8>(1);
    test_transform_width<6, 8>(2);
    test_transform_width<6, 8>(3);
    test_transform_width<6, 8>(4);
    test_transform_width<6, 8>(5);
    test_transform_width<6, 8>(6);
    test_transform_width<6, 8>(7);
    test_transform_width<6, 8>(8);

    return EXIT_SUCCESS;
}
