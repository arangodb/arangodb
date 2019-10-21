// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/parameters.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/deduced.hpp>
#include <boost/parameter/name.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/type_traits/is_convertible.hpp>

BOOST_PARAMETER_NAME(x)

int main()
{
    boost::parameter::parameters<
        boost::parameter::optional<
            boost::parameter::deduced<tag::x>
          , boost::is_convertible<boost::mpl::_,int>
        >
    >()("foo");
    return 0;
}

