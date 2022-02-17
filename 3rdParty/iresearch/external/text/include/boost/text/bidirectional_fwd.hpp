// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_BIDIRECTIONAL_FWD_HPP
#define BOOST_TEXT_BIDIRECTIONAL_FWD_HPP

#include <cstdint>


namespace boost { namespace text {

    /** The bidirectional algorithm properties defined by Unicode. */
    enum class bidi_property : uint8_t {
        L,
        R,
        EN,
        ES,
        ET,
        AN,
        CS,
        B,
        S,
        WS,
        ON,
        BN,
        NSM,
        AL,
        LRO,
        RLO,
        LRE,
        RLE,
        PDF,
        LRI,
        RLI,
        FSI,
        PDI
    };

#ifdef BOOST_TEXT_TESTING
    std::ostream & operator<<(std::ostream & os, bidi_property prop)
    {
#define CASE(x)                                                                \
        case bidi_property::x: os << #x; break
        switch (prop) {
            CASE(L);
            CASE(R);
            CASE(EN);
            CASE(ES);
            CASE(ET);
            CASE(AN);
            CASE(CS);
            CASE(B);
            CASE(S);
            CASE(WS);
            CASE(ON);
            CASE(BN);
            CASE(NSM);
            CASE(AL);
            CASE(LRO);
            CASE(RLO);
            CASE(LRE);
            CASE(RLE);
            CASE(PDF);
            CASE(LRI);
            CASE(RLI);
            CASE(FSI);
            CASE(PDI);
        }
#undef CASE
        return os;
    }
#endif

}}

#endif
