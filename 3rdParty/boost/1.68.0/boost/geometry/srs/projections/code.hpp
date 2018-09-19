// Boost.Geometry

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_CODE_HPP
#define BOOST_GEOMETRY_PROJECTIONS_CODE_HPP


#include <algorithm>
#include <string>


namespace boost { namespace geometry { namespace projections
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

    struct code_element
    {
        int code;
        std::string proj4_str;
    };

    struct code_element_less
    {
        inline bool operator()(code_element const& l, code_element const& r) const
        {
            return l.code < r.code;
        }
    };

    template<typename RandIt>
    inline RandIt binary_find_code_element(RandIt first, RandIt last, int code)
    {
        code_element_less comp;
        code_element value;
        value.code = code;
        first = std::lower_bound(first, last, value, code_element_less());
        return first != last && !comp(value, *first) ? first : last;
    }

}
#endif // DOXYGEN_NO_DETAIL


}}} // namespace boost::geometry::projections

#endif
