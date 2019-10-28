//  Copyright (c) 2018-2019 Cem Bassoy
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  The authors gratefully acknowledge the support of
//  Fraunhofer and Google in producing this work
//  which started as a Google Summer of Code project.
//




#include <boost/numeric/ublas/tensor/expression.hpp>
#include <boost/numeric/ublas/tensor/tensor.hpp>
#include <boost/test/unit_test.hpp>
#include "utility.hpp"

#include <functional>
#include <complex>



using test_types = zip<int,long,float,double,std::complex<float>>::with_t<boost::numeric::ublas::first_order, boost::numeric::ublas::last_order>;


struct fixture
{
	using extents_type = boost::numeric::ublas::shape;
	fixture()
	  : extents {
	      extents_type{},            // 0

	      extents_type{1,1},         // 1
	      extents_type{1,2},         // 2
	      extents_type{2,1},         // 3

	      extents_type{2,3},         // 4
	      extents_type{2,3,1},       // 5
	      extents_type{1,2,3},       // 6
	      extents_type{1,1,2,3},     // 7
	      extents_type{1,2,3,1,1},   // 8

	      extents_type{4,2,3},       // 9
	      extents_type{4,2,1,3},     // 10
	      extents_type{4,2,1,3,1},   // 11
	      extents_type{1,4,2,1,3,1} }  // 12
	{
	}
	std::vector<extents_type> extents;
};


BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_expression_access, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using tensor_expression_type  = typename tensor_type::super_type;


	for(auto const& e : extents) {

		auto v = value_type{};
		auto t = tensor_type(e);

		for(auto& tt: t){ tt = v; v+=value_type{1}; }
		const auto& tensor_expression_const = static_cast<tensor_expression_type const&>( t );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( tensor_expression_const()(i), t(i)  );

	}
}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_unary_expression, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;

	auto uplus1 = std::bind(  std::plus<value_type>{}, std::placeholders::_1, value_type(1) );

	for(auto const& e : extents) {

		auto t = tensor_type(e);
		auto v = value_type{};
		for(auto& tt: t) { tt = v; v+=value_type{1}; }

		const auto uexpr = ublas::detail::make_unary_tensor_expression<tensor_type>( t, uplus1 );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( uexpr(i), uplus1(t(i))  );

		auto uexpr_uexpr = ublas::detail::make_unary_tensor_expression<tensor_type>( uexpr, uplus1 );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( uexpr_uexpr(i), uplus1(uplus1(t(i)))  );

		const auto & uexpr_e = uexpr.e;

		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(uexpr_e) >, tensor_type > )   );

		const auto & uexpr_uexpr_e_e = uexpr_uexpr.e.e;

		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(uexpr_uexpr_e_e) >, tensor_type > )   );


	}
}


BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_binary_expression, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;

	auto uplus1 = std::bind(  std::plus<value_type>{}, std::placeholders::_1, value_type(1) );
	auto uplus2 = std::bind(  std::plus<value_type>{}, std::placeholders::_1, value_type(2) );
	auto bplus  = std::plus <value_type>{};
	auto bminus = std::minus<value_type>{};

	for(auto const& e : extents) {

		auto t = tensor_type(e);
		auto v = value_type{};
		for(auto& tt: t){ tt = v; v+=value_type{1}; }

		auto uexpr1 = ublas::detail::make_unary_tensor_expression<tensor_type>( t, uplus1 );
		auto uexpr2 = ublas::detail::make_unary_tensor_expression<tensor_type>( t, uplus2 );

		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(uexpr1.e) >, tensor_type > )   );
		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(uexpr2.e) >, tensor_type > )   );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( uexpr1(i), uplus1(t(i))  );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( uexpr2(i), uplus2(t(i))  );

		auto bexpr_uexpr = ublas::detail::make_binary_tensor_expression<tensor_type>( uexpr1, uexpr2, bplus );

		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_uexpr.el.e) >, tensor_type > )   );
		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_uexpr.er.e) >, tensor_type > )   );


		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( bexpr_uexpr(i), bplus(uexpr1(i),uexpr2(i))  );

		auto bexpr_bexpr_uexpr = ublas::detail::make_binary_tensor_expression<tensor_type>( bexpr_uexpr, t, bminus );

		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_bexpr_uexpr.el.el.e) >, tensor_type > )   );
		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_bexpr_uexpr.el.er.e) >, tensor_type > )   );
		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_bexpr_uexpr.er) >, tensor_type > )   );
		BOOST_CHECK( ( std::is_same_v< std::decay_t< decltype(bexpr_bexpr_uexpr.er) >, tensor_type > )   );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL( bexpr_bexpr_uexpr(i), bminus(bexpr_uexpr(i),t(i))  );

	}


}
