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


#include <iostream>
#include <random>
#include <boost/numeric/ublas/tensor.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/test/unit_test.hpp>

#include "utility.hpp"

// BOOST_AUTO_TEST_SUITE ( test_tensor_matrix_interoperability, * boost::unit_test::depends_on("test_tensor") ) ;

BOOST_AUTO_TEST_SUITE ( test_tensor_matrix_interoperability )

using test_types = zip<int,long,float,double>::with_t<boost::numeric::ublas::first_order, boost::numeric::ublas::last_order>;


BOOST_AUTO_TEST_CASE_TEMPLATE( test_tensor_matrix_copy_ctor, value,  test_types)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;

	tensor_type a1 = matrix_type();
	BOOST_CHECK_EQUAL( a1.size() , 0ul );
	BOOST_CHECK( a1.empty() );
	BOOST_CHECK_EQUAL( a1.data() , nullptr);

	tensor_type a2 = matrix_type(1,1);
	BOOST_CHECK_EQUAL(  a2.size() , 1 );
	BOOST_CHECK( !a2.empty() );
	BOOST_CHECK_NE(  a2.data() , nullptr);

	tensor_type a3 = matrix_type(2,1);
	BOOST_CHECK_EQUAL(  a3.size() , 2 );
	BOOST_CHECK( !a3.empty() );
	BOOST_CHECK_NE(  a3.data() , nullptr);

	tensor_type a4 = matrix_type(1,2);
	BOOST_CHECK_EQUAL(  a4.size() , 2 );
	BOOST_CHECK( !a4.empty() );
	BOOST_CHECK_NE(  a4.data() , nullptr);

	tensor_type a5 = matrix_type(2,3);
	BOOST_CHECK_EQUAL(  a5.size() , 6 );
	BOOST_CHECK( !a5.empty() );
	BOOST_CHECK_NE(  a5.data() , nullptr);
}


BOOST_AUTO_TEST_CASE_TEMPLATE( test_tensor_vector_copy_ctor, value,  test_types)
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using vector_type = typename tensor_type::vector_type;

	tensor_type a1 = vector_type();
	BOOST_CHECK_EQUAL( a1.size() , 0ul );
	BOOST_CHECK( a1.empty() );
	BOOST_CHECK_EQUAL( a1.data() , nullptr);

	tensor_type a2 = vector_type(1);
	BOOST_CHECK_EQUAL(  a2.size() , 1 );
	BOOST_CHECK( !a2.empty() );
	BOOST_CHECK_NE(  a2.data() , nullptr);

	tensor_type a3 = vector_type(2);
	BOOST_CHECK_EQUAL(  a3.size() , 2 );
	BOOST_CHECK( !a3.empty() );
	BOOST_CHECK_NE(  a3.data() , nullptr);

	tensor_type a4 = vector_type(2);
	BOOST_CHECK_EQUAL(  a4.size() , 2 );
	BOOST_CHECK( !a4.empty() );
	BOOST_CHECK_NE(  a4.data() , nullptr);

	tensor_type a5 = vector_type(3);
	BOOST_CHECK_EQUAL(  a5.size() , 3 );
	BOOST_CHECK( !a5.empty() );
	BOOST_CHECK_NE(  a5.data() , nullptr);
}


struct fixture
{
	using extents_type = boost::numeric::ublas::basic_extents<std::size_t>;
	fixture()
	  : extents{
				extents_type{1,1},  // 1
				extents_type{1,2},  // 2
				extents_type{2,1},  // 3
				extents_type{2,3},  // 4
				extents_type{9,7},  // 5
				extents_type{9,11}, // 6
				extents_type{12,12}, // 7
	      extents_type{15,17}} // 8
	{
	}
	std::vector<extents_type> extents;
};




BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_matrix_copy_ctor_extents, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;

	auto check = [](auto const& e) {
		assert(e.size()==2);
		tensor_type t = matrix_type{e[0],e[1]};
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);
	};

	for(auto const& e : extents)
		check(e);
}


BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_vector_copy_ctor_extents, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using vector_type = typename tensor_type::vector_type;

	auto check = [](auto const& e) {
		assert(e.size()==2);
		if(e.empty())
			return;

		tensor_type t = vector_type(e.product());
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);
	};

	for(auto const& e : extents)
		check(e);
}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_matrix_copy_assignment, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = matrix_type(e[0],e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		t = r;

		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0) );
		BOOST_CHECK_EQUAL (  t.extents().at(1) , e.at(1) );
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		for(auto j = 0ul; j < t.size(1); ++j){
			for(auto i = 0ul; i < t.size(0); ++i){
				BOOST_CHECK_EQUAL( t.at(i,j), r(i,j)  );
			}
		}
	};

	for(auto const& e : extents)
		check(e);
}


BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_vector_copy_assignment, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using vector_type = typename tensor_type::vector_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = vector_type(e[0]*e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		t = r;

		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0)*e.at(1) );
		BOOST_CHECK_EQUAL (  t.extents().at(1) , 1);
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		for(auto i = 0ul; i < t.size(); ++i){
			BOOST_CHECK_EQUAL( t[i], r(i)  );
		}
	};

	for(auto const& e : extents)
		check(e);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_matrix_move_assignment, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = matrix_type(e[0],e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		auto q = r;
		t = std::move(r);

		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0) );
		BOOST_CHECK_EQUAL (  t.extents().at(1) , e.at(1) );
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		for(auto j = 0ul; j < t.size(1); ++j){
			for(auto i = 0ul; i < t.size(0); ++i){
				BOOST_CHECK_EQUAL( t.at(i,j), q(i,j)  );
			}
		}
	};

	for(auto const& e : extents)
		check(e);
}




BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_vector_move_assignment, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using vector_type = typename tensor_type::vector_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = vector_type(e[0]*e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		auto q = r;
		t = std::move(r);

		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0) * e.at(1));
		BOOST_CHECK_EQUAL (  t.extents().at(1) , 1);
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		for(auto i = 0ul; i < t.size(); ++i){
			BOOST_CHECK_EQUAL( t[i], q(i)  );
		}
	};

	for(auto const& e : extents)
		check(e);
}





BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_matrix_expressions, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = matrix_type(e[0],e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		t = r + 3*r;
		tensor_type s = r + 3*r;
		tensor_type q = s + r + 3*r + s; // + 3*r


		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0) );
		BOOST_CHECK_EQUAL (  t.extents().at(1) , e.at(1) );
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		BOOST_CHECK_EQUAL (  s.extents().at(0) , e.at(0) );
		BOOST_CHECK_EQUAL (  s.extents().at(1) , e.at(1) );
		BOOST_CHECK_EQUAL (  s.size() , e.product() );
		BOOST_CHECK_EQUAL (  s.rank() , e.size() );
		BOOST_CHECK       ( !s.empty()    );
		BOOST_CHECK_NE    (  s.data() , nullptr);

		BOOST_CHECK_EQUAL (  q.extents().at(0) , e.at(0) );
		BOOST_CHECK_EQUAL (  q.extents().at(1) , e.at(1) );
		BOOST_CHECK_EQUAL (  q.size() , e.product() );
		BOOST_CHECK_EQUAL (  q.rank() , e.size() );
		BOOST_CHECK       ( !q.empty()    );
		BOOST_CHECK_NE    (  q.data() , nullptr);


		for(auto j = 0ul; j < t.size(1); ++j){
			for(auto i = 0ul; i < t.size(0); ++i){
				BOOST_CHECK_EQUAL( t.at(i,j), 4*r(i,j)  );
				BOOST_CHECK_EQUAL( s.at(i,j), t.at(i,j)  );
				BOOST_CHECK_EQUAL( q.at(i,j), 3*s.at(i,j)  );
			}
		}
	};

	for(auto const& e : extents)
		check(e);
}






BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_vector_expressions, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using vector_type = typename tensor_type::vector_type;

	auto check = [](auto const& e)
	{
		assert(e.size() == 2);
		auto t = tensor_type{};
		auto r = vector_type(e[0]*e[1]);
		std::iota(r.data().begin(),r.data().end(), 1);
		t = r + 3*r;
		tensor_type s = r + 3*r;
		tensor_type q = s + r + 3*r + s; // + 3*r


		BOOST_CHECK_EQUAL (  t.extents().at(0) , e.at(0)*e.at(1) );
		BOOST_CHECK_EQUAL (  t.extents().at(1) , 1);
		BOOST_CHECK_EQUAL (  t.size() , e.product() );
		BOOST_CHECK_EQUAL (  t.rank() , e.size() );
		BOOST_CHECK       ( !t.empty()    );
		BOOST_CHECK_NE    (  t.data() , nullptr);

		BOOST_CHECK_EQUAL (  s.extents().at(0) , e.at(0)*e.at(1) );
		BOOST_CHECK_EQUAL (  s.extents().at(1) , 1);
		BOOST_CHECK_EQUAL (  s.size() , e.product() );
		BOOST_CHECK_EQUAL (  s.rank() , e.size() );
		BOOST_CHECK       ( !s.empty()    );
		BOOST_CHECK_NE    (  s.data() , nullptr);

		BOOST_CHECK_EQUAL (  q.extents().at(0) , e.at(0)*e.at(1) );
		BOOST_CHECK_EQUAL (  q.extents().at(1) , 1);
		BOOST_CHECK_EQUAL (  q.size() , e.product() );
		BOOST_CHECK_EQUAL (  q.rank() , e.size() );
		BOOST_CHECK       ( !q.empty()    );
		BOOST_CHECK_NE    (  q.data() , nullptr);



		for(auto i = 0ul; i < t.size(); ++i){
			BOOST_CHECK_EQUAL( t.at(i), 4*r(i)  );
			BOOST_CHECK_EQUAL( s.at(i), t.at(i)  );
			BOOST_CHECK_EQUAL( q.at(i), 3*s.at(i)  );
		}
	};

	for(auto const& e : extents)
		check(e);
}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_matrix_vector_expressions, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using tensor_type = ublas::tensor<value_type, layout_type>;
	using matrix_type = typename tensor_type::matrix_type;
	using vector_type = typename tensor_type::vector_type;

	auto check = [](auto const& e)
	{
		if(e.product() <= 2)
			return;
		assert(e.size() == 2);
		auto Q = tensor_type{e[0],1};
		auto A = matrix_type(e[0],e[1]);
		auto b = vector_type(e[1]);
		auto c = vector_type(e[0]);
		std::iota(b.data().begin(),b.data().end(), 1);
		std::fill(A.data().begin(),A.data().end(), 1);
		std::fill(c.data().begin(),c.data().end(), 2);
		std::fill(Q.begin(),Q.end(), 2);

		tensor_type T = Q + (ublas::prod(A , b) + 2*c) + 3*Q;

		BOOST_CHECK_EQUAL (  T.extents().at(0) , Q.extents().at(0) );
		BOOST_CHECK_EQUAL (  T.extents().at(1) , Q.extents().at(1));
		BOOST_CHECK_EQUAL (  T.size() , Q.size() );
		BOOST_CHECK_EQUAL (  T.size() , c.size() );
		BOOST_CHECK_EQUAL (  T.rank() , Q.rank() );
		BOOST_CHECK       ( !T.empty()    );
		BOOST_CHECK_NE    (  T.data() , nullptr);

		for(auto i = 0ul; i < T.size(); ++i){
			auto n = e[1];
			auto ab = n * (n+1) / 2;
			BOOST_CHECK_EQUAL( T(i), ab+4*Q(0)+2*c(0)  );
		}

	};



	for(auto const& e : extents)
		check(e);
}


BOOST_AUTO_TEST_SUITE_END()

