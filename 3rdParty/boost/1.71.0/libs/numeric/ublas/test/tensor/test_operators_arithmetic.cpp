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



#include <boost/numeric/ublas/tensor.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include "utility.hpp"


using double_extended = boost::multiprecision::cpp_bin_float_double_extended;

using test_types = zip<int,long,float,double,double_extended>::with_t<boost::numeric::ublas::first_order, boost::numeric::ublas::last_order>;

struct fixture
{
	using extents_type = boost::numeric::ublas::basic_extents<std::size_t>;
	fixture()
	  : extents{
	      extents_type{},    // 0
	      extents_type{1,1}, // 1
	      extents_type{1,2}, // 2
	      extents_type{2,1}, // 3
	      extents_type{2,3}, // 4
	      extents_type{2,3,1}, // 5
	      extents_type{4,1,3}, // 6
	      extents_type{1,2,3}, // 7
	      extents_type{4,2,3}, // 8
	      extents_type{4,2,3,5}} // 9
	{
	}
	std::vector<extents_type> extents;
};

BOOST_AUTO_TEST_SUITE(test_tensor_arithmetic_operations, * boost::unit_test::depends_on("test_tensor"))



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_binary_arithmetic_operations, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;


	auto check = [](auto const& e)
	{
		auto t  = tensor_type (e);
		auto t2 = tensor_type (e);
		auto r  = tensor_type (e);
		auto v  = value_type  {};

		std::iota(t.begin(), t.end(), v);
		std::iota(t2.begin(), t2.end(), v+2);

		r = t + t + t + t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 3*t(i) + t2(i) );


		r = t2 / (t+3) * (t+1) - t2; // r = ( t2/ ((t+3)*(t+1)) ) - t2

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), t2(i) / (t(i)+3)*(t(i)+1) - t2(i) );

		r = 3+t2 / (t+3) * (t+1) * t - t2; // r = 3+( t2/ ((t+3)*(t+1)*t) ) - t2

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 3+t2(i) / (t(i)+3)*(t(i)+1)*t(i) - t2(i) );

		r = t2 - t + t2 - t;

		for(auto i = 0ul; i < r.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 4 );


		r = tensor_type (e,1) + tensor_type (e,1);

		for(auto i = 0ul; i < r.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2 );

		r = t * t * t * t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), t(i)*t(i)*t(i)*t2(i) );

		r = (t2/t2) * (t2/t2);

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 1 );
	};

	for(auto const& e : extents)
		check(e);


	BOOST_CHECK_NO_THROW ( tensor_type t = tensor_type(extents.at(0)) + tensor_type(extents.at(0))  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(0)) + tensor_type(extents.at(2)), std::runtime_error  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(1)) + tensor_type(extents.at(2)), std::runtime_error  );


}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_unary_arithmetic_operations, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;


	auto check = [](auto const& e)
	{
		auto t  = tensor_type (e);
		auto t2 = tensor_type (e);
		auto v  = value_type  {};

		std::iota(t.begin(), t.end(), v);
		std::iota(t2.begin(), t2.end(), v+2);

		tensor_type r1 = t + 2 + t + 2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r1(i), 2*t(i) + 4 );

		tensor_type r2 = 2 + t + 2 + t;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r2(i), 2*t(i) + 4 );

		tensor_type r3 = (t-2) + (t-2);

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r3(i), 2*t(i) - 4 );

		tensor_type r4 = (t*2) * (3*t);

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r4(i), 2*3*t(i)*t(i) );

		tensor_type r5 = (t2*2) / (2*t2) * t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r5(i), (t2(i)*2) / (2*t2(i)) * t2(i) );

		tensor_type r6 = (t2/2+1) / (2/t2+1) / t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r6(i), (t2(i)/2+1) / (2/t2(i)+1) / t2(i) );

	};

	for(auto const& e : extents)
		check(e);

	BOOST_CHECK_NO_THROW ( tensor_type t = tensor_type(extents.at(0)) + 2 + tensor_type(extents.at(0))  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(0)) + 2 + tensor_type(extents.at(2)), std::runtime_error  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(1)) + 2 + tensor_type(extents.at(2)), std::runtime_error  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(2)) + tensor_type(extents.at(1)), std::runtime_error  );
	BOOST_CHECK_THROW    ( tensor_type t = tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(1)), std::runtime_error  );
}





BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_assign_arithmetic_operations, value,  test_types, fixture)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;


	auto check = [](auto const& e)
	{
		auto t  = tensor_type (e);
		auto t2 = tensor_type (e);
		auto r  = tensor_type (e);
		auto v  = value_type  {};

		std::iota(t.begin(), t.end(), v);
		std::iota(t2.begin(), t2.end(), v+2);

		r  = t + 2;
		r += t;
		r += 2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2*t(i) + 4 );

		r  = 2 + t;
		r += t;
		r += 2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2*t(i) + 4 );

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2*t(i) + 4 );

		r = (t-2);
		r += t;
		r -= 2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2*t(i) - 4 );

		r  = (t*2);
		r *= 3;
		r *= t;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), 2*3*t(i)*t(i) );

		r  = (t2*2);
		r /= 2;
		r /= t2;
		r *= t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), (t2(i)*2) / (2*t2(i)) * t2(i) );

		r  = (t2/2+1);
		r /= (2/t2+1);
		r /= t2;

		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( r(i), (t2(i)/2+1) / (2/t2(i)+1) / t2(i) );

		tensor_type q = -r;
		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( q(i), -r(i) );

		tensor_type p = +r;
		for(auto i = 0ul; i < t.size(); ++i)
			BOOST_CHECK_EQUAL ( p(i), r(i) );
	};

	for(auto const& e : extents)
		check(e);

	auto r  = tensor_type (extents.at(0));

	BOOST_CHECK_NO_THROW ( r += tensor_type(extents.at(0)) + 2 + tensor_type(extents.at(0))  );
	BOOST_CHECK_THROW    ( r += tensor_type(extents.at(0)) + 2 + tensor_type(extents.at(2)), std::runtime_error  );
	BOOST_CHECK_THROW    ( r += tensor_type(extents.at(1)) + 2 + tensor_type(extents.at(2)), std::runtime_error  );
	BOOST_CHECK_THROW    ( r += tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(2)) + tensor_type(extents.at(1)), std::runtime_error  );
	BOOST_CHECK_THROW    ( r += tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(2)) + 2 + tensor_type(extents.at(1)), std::runtime_error  );
}


BOOST_AUTO_TEST_SUITE_END()
