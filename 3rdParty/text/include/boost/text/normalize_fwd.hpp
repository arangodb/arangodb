// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_NORMALIZE_FWD_HPP
#define BOOST_TEXT_NORMALIZE_FWD_HPP

#include <boost/text/config.hpp>


namespace boost { namespace text {

    /** The Unicode Normalization Forms.

        \see https://unicode.org/reports/tr15 */
    enum class nf {
        c,   // NFC
        d,   // NFD
        kc,  // NFKC
        kd,  // NFKD
        fcc, // An unofficial normalization form similar to NFC.
    };

    /** An enumeration used with the container `insert()` free functions, to
        indicate whether the inserted text is known to be normalized. */
    enum insertion_normalization {
        insertion_normalized,    // The inserted text is known to be in the
                                 // correct normalization form, and in
                                 // stream-safe format.
        insertion_not_normalized // The inserted text is in an unknown
                                 // normalization form.
    };

    /** Expands to a `static_assert` that `Normalization` is a valid
        enumerator from `nf`. */
#define BOOST_TEXT_STATIC_ASSERT_NORMALIZATION()                               \
    static_assert(                                                             \
        (int)nf::c <= (int)Normalization &&                                    \
            (int)Normalization <= (int)nf::fcc,                                \
        "Invalid normalization form.")

}}

#endif
