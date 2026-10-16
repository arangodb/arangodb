// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/normalize_string.hpp>


int main ()
{

{
//[ normalize_1
// 쨰◌̴ᆮ HANGUL SYLLABLE JJYAE, COMBINING TILDE OVERLAY, HANGUL JONGSEONG TIKEUT
std::array<uint32_t, 4> const nfd = {{ 0x110D, 0x1164, 0x0334, 0x11AE }};
// Iterator interface.
assert(boost::text::normalized<boost::text::nf::d>(nfd.begin(), nfd.end()));

{
    std::vector<uint32_t> nfc;
    // Iterator interface.
    boost::text::normalize<boost::text::nf::c>(nfd.begin(), nfd.end(), std::back_inserter(nfc));
    // Range interface.
    assert(boost::text::normalized<boost::text::nf::c>(nfc));
}

{
    std::vector<uint32_t> nfc;
    // Range interface.
    boost::text::normalize<boost::text::nf::c>(nfd, std::back_inserter(nfc));
    // Range interface.
    assert(boost::text::normalized<boost::text::nf::c>(nfc));
}
//]
}

{
//[ normalize_2
// 쨰◌̴ᆮ HANGUL SYLLABLE JJYAE, COMBINING TILDE OVERLAY, HANGUL JONGSEONG TIKEUT
std::array<uint32_t, 4> const nfd = {{ 0x110D, 0x1164, 0x0334, 0x11AE }};
// Iterator interface.
assert(boost::text::normalized<boost::text::nf::d>(nfd.begin(), nfd.end()));

{
    std::vector<uint16_t> nfc;
    // Iterator interface.
    boost::text::normalize_append<boost::text::nf::c>(nfd.begin(), nfd.end(), nfc);
    // Range interface.
    assert(boost::text::normalized<boost::text::nf::c>(boost::text::as_utf32(nfc)));
}

{
    std::vector<char> nfc;
    // Range interface.
    boost::text::normalize_append<boost::text::nf::c>(nfd, nfc);
    // Range interface.
    assert(boost::text::normalized<boost::text::nf::c>(boost::text::as_utf32(nfc)));
}
//]
}

}
