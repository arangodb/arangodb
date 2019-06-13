/*=============================================================================
    Copyright (c) 2011,2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_SYNTAX_HIGHLIGHT_HPP)
#define BOOST_QUICKBOOK_SYNTAX_HIGHLIGHT_HPP

#include <boost/swap.hpp>
#include "fwd.hpp"
#include "iterator.hpp"
#include "phrase_tags.hpp"

namespace quickbook
{
    //
    // source_mode_info
    //
    // The source mode is stored in a few places, so the order needs to also be
    // stored to work out which is the current source mode.

    struct source_mode_info
    {
        source_mode_type source_mode;
        unsigned order;

        source_mode_info() : source_mode(source_mode_tags::cpp), order(0) {}

        source_mode_info(source_mode_type source_mode_, unsigned order_)
            : source_mode(source_mode_), order(order_)
        {
        }

        void update(source_mode_info const& x)
        {
            if (x.order > order) {
                source_mode = x.source_mode;
                order = x.order;
            }
        }

        void swap(source_mode_info& x)
        {
            boost::swap(source_mode, x.source_mode);
            boost::swap(order, x.order);
        }
    };

    inline void swap(source_mode_info& x, source_mode_info& y) { x.swap(y); }

    void syntax_highlight(
        parse_iterator first,
        parse_iterator last,
        quickbook::state& state,
        source_mode_type source_mode,
        bool is_block);
}

#endif
