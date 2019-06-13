/*=============================================================================
    Copyright (c) 2017 Daniel James
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_QUICKBOOK_STRING_VIEW_HPP)
#define BOOST_SPIRIT_QUICKBOOK_STRING_VIEW_HPP

#include <boost/functional/hash/hash_fwd.hpp>
#include <boost/utility/string_view.hpp>

namespace quickbook
{
    // boost::string_view now can't be constructed from an rvalue std::string,
    // which is something that quickbook does in several places, so this wraps
    // it to allow that.

    struct string_view : boost::string_view
    {
        typedef boost::string_view base;

        string_view() : base() {}
        string_view(string_view const& x) : base(x) {}
        string_view(std::string const& x) : base(x) {}
        string_view(const char* x) : base(x) {}
        string_view(const char* x, base::size_type len) : base(x, len) {}

        std::string to_s() const { return std::string(begin(), end()); }
    };

    typedef quickbook::string_view::const_iterator string_iterator;

    inline std::size_t hash_value(string_view const& x)
    {
        return boost::hash_range(x.begin(), x.end());
    }

    inline std::string& operator+=(std::string& x, string_view const& y)
    {
        return x.append(y.begin(), y.end());
    }
}

#endif
