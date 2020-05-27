//  Copyright (c) 2018-2019
//  Cem Bassoy
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  The authors gratefully acknowledge the support of
//  Fraunhofer and Google in producing this work
//  which started as a Google Summer of Code project.
//

#ifndef _BOOST_UBLAS_TEST_TENSOR_UTILITY_
#define _BOOST_UBLAS_TEST_TENSOR_UTILITY_

template<class ... types>
struct zip_helper;

template<class type1, class ... types3>
struct zip_helper<std::tuple<types3...>, type1>
{
	template<class ... types2>
	struct with
	{
		using type = std::tuple<types3...,std::pair<type1,types2>...>;
	};
	template<class ... types2>
	using with_t = typename with<types2...>::type;
};


template<class type1, class ... types3, class ... types1>
struct zip_helper<std::tuple<types3...>, type1, types1...>
{
	template<class ... types2>
	struct with
	{
		using next_tuple = std::tuple<types3...,std::pair<type1,types2>...>;
		using type       = typename zip_helper<next_tuple, types1...>::template with<types2...>::type;
	};

	template<class ... types2>
	using with_t = typename with<types2...>::type;
};

template<class ... types>
using zip = zip_helper<std::tuple<>,types...>;

// creates e.g.
// using test_types = zip<long,float>::with_t<first_order,last_order>; // equals
// using test_types = std::tuple< std::pair<float, first_order>, std::pair<float, last_order >, std::pair<double,first_order>, std::pair<double,last_order >
//>;
//static_assert(std::is_same< std::tuple_element_t<0,std::tuple_element_t<0,test_types2>>, float>::value,"should be float ");
//static_assert(std::is_same< std::tuple_element_t<1,std::tuple_element_t<0,test_types2>>, boost::numeric::ublas::first_order>::value,"should be boost::numeric::ublas::first_order ");

#endif
