//-----------------------------------------------------------------------------
// boost-libs variant/test/issue42.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Test case from https://github.com/boostorg/variant/issues/42

#include <boost/variant.hpp>
#include <map>
#include <memory>
#include <vector>



#ifdef BOOST_NO_CXX11_SMART_PTR
    template <class T> struct shared_ptr_like {};
    typedef shared_ptr_like<boost::recursive_variant_> ptr_t;
#else
    typedef std::shared_ptr<boost::recursive_variant_> ptr_t;
#endif

template <class F>
class func{};

int main() {
    typedef boost::make_recursive_variant<
        int,
        ptr_t
    >::type node;

    node x = 1;
    (void)x;


    typedef boost::make_recursive_variant<
        std::string, int, double, bool,
        ptr_t,
        std::map<const std::string, boost::recursive_variant_>,
        std::vector<boost::recursive_variant_>
    >::type node2;

    node2 x2 = 1;
    (void)x2;


    typedef boost::make_recursive_variant<
        int,
        func<boost::recursive_variant_(*)(boost::recursive_variant_&, const boost::recursive_variant_&)>,
        boost::recursive_variant_&(*)(boost::recursive_variant_, boost::recursive_variant_*),
        ptr_t
    >::type node3;

    node3 x3 = func<node3(*)(node3&, const node3&)>();
    (void)x3;
}
