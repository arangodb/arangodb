//Copyright (c) 2006-2015 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "boost/exception/info.hpp"

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
int
main()
	{
	return 0;
	}
#else
template <class I>
I const
rv_const( I i )
	{
	return i;
	}
template <class E,class I>
E const &
add_info( E const & e, I i )
	{
	return e << rv_const(i);
	}
#include "error_info_test.hpp"
#endif
