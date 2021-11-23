//  Copyright (c) 2018-2019 Cem Bassoy
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>
#include <boost/numeric/ublas/tensor/extents.hpp>
#include <vector>

BOOST_AUTO_TEST_SUITE ( test_extents )


//*boost::unit_test::label("extents")
//*boost::unit_test::label("constructor")

BOOST_AUTO_TEST_CASE(test_extents_ctor)
{
	using namespace boost::numeric;
	using extents = ublas::basic_extents<unsigned>;


	auto e0 = extents{};
	BOOST_CHECK( e0.empty());
	BOOST_CHECK_EQUAL ( e0.size(),0);

	auto e1 = extents{1,1};
	BOOST_CHECK(!e1.empty());
	BOOST_CHECK_EQUAL ( e1.size(),2);

	auto e2 = extents{1,2};
	BOOST_CHECK(!e2.empty());
	BOOST_CHECK_EQUAL ( e2.size(),2);

	auto e3 = extents{2,1};
	BOOST_CHECK (!e3.empty());
	BOOST_CHECK_EQUAL  ( e3.size(),2);

	auto e4 = extents{2,3};
	BOOST_CHECK(!e4.empty());
	BOOST_CHECK_EQUAL ( e4.size(),2);

	auto e5 = extents{2,3,1};
	BOOST_CHECK (!e5.empty());
	BOOST_CHECK_EQUAL  ( e5.size(),3);

	auto e6 = extents{1,2,3}; // 6
	BOOST_CHECK(!e6.empty());
	BOOST_CHECK_EQUAL ( e6.size(),3);

	auto e7 = extents{4,2,3};  // 7
	BOOST_CHECK(!e7.empty());
	BOOST_CHECK_EQUAL ( e7.size(),3);

	BOOST_CHECK_THROW( extents({1,0}), std::length_error );
	BOOST_CHECK_THROW( extents({0}  ), std::length_error );
	BOOST_CHECK_THROW( extents({3}  ), std::length_error );
	BOOST_CHECK_THROW( extents({0,1}), std::length_error );
}




struct fixture {
	using extents_type = boost::numeric::ublas::basic_extents<unsigned>;
	fixture() : extents{
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
	              extents_type{1,4,2,1,3,1}, // 12
}  // 13
	{}
	std::vector<extents_type> extents;
};

BOOST_FIXTURE_TEST_CASE(test_extents_access, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("access"))
{
	using namespace boost::numeric;

	BOOST_REQUIRE_EQUAL(extents.size(),13);

	BOOST_CHECK_EQUAL  (extents[ 0].size(), 0);
	BOOST_CHECK (extents[ 0].empty()  );

	BOOST_REQUIRE_EQUAL(extents[ 1].size(), 2);
	BOOST_REQUIRE_EQUAL(extents[ 2].size(), 2);
	BOOST_REQUIRE_EQUAL(extents[ 3].size(), 2);
	BOOST_REQUIRE_EQUAL(extents[ 4].size(), 2);
	BOOST_REQUIRE_EQUAL(extents[ 5].size(), 3);
	BOOST_REQUIRE_EQUAL(extents[ 6].size(), 3);
	BOOST_REQUIRE_EQUAL(extents[ 7].size(), 4);
	BOOST_REQUIRE_EQUAL(extents[ 8].size(), 5);
	BOOST_REQUIRE_EQUAL(extents[ 9].size(), 3);
	BOOST_REQUIRE_EQUAL(extents[10].size(), 4);
	BOOST_REQUIRE_EQUAL(extents[11].size(), 5);
	BOOST_REQUIRE_EQUAL(extents[12].size(), 6);


	BOOST_CHECK_EQUAL(extents[1][0],1);
	BOOST_CHECK_EQUAL(extents[1][1],1);

	BOOST_CHECK_EQUAL(extents[2][0],1);
	BOOST_CHECK_EQUAL(extents[2][1],2);

	BOOST_CHECK_EQUAL(extents[3][0],2);
	BOOST_CHECK_EQUAL(extents[3][1],1);

	BOOST_CHECK_EQUAL(extents[4][0],2);
	BOOST_CHECK_EQUAL(extents[4][1],3);

	BOOST_CHECK_EQUAL(extents[5][0],2);
	BOOST_CHECK_EQUAL(extents[5][1],3);
	BOOST_CHECK_EQUAL(extents[5][2],1);

	BOOST_CHECK_EQUAL(extents[6][0],1);
	BOOST_CHECK_EQUAL(extents[6][1],2);
	BOOST_CHECK_EQUAL(extents[6][2],3);

	BOOST_CHECK_EQUAL(extents[7][0],1);
	BOOST_CHECK_EQUAL(extents[7][1],1);
	BOOST_CHECK_EQUAL(extents[7][2],2);
	BOOST_CHECK_EQUAL(extents[7][3],3);

	BOOST_CHECK_EQUAL(extents[8][0],1);
	BOOST_CHECK_EQUAL(extents[8][1],2);
	BOOST_CHECK_EQUAL(extents[8][2],3);
	BOOST_CHECK_EQUAL(extents[8][3],1);
	BOOST_CHECK_EQUAL(extents[8][4],1);

	BOOST_CHECK_EQUAL(extents[9][0],4);
	BOOST_CHECK_EQUAL(extents[9][1],2);
	BOOST_CHECK_EQUAL(extents[9][2],3);

	BOOST_CHECK_EQUAL(extents[10][0],4);
	BOOST_CHECK_EQUAL(extents[10][1],2);
	BOOST_CHECK_EQUAL(extents[10][2],1);
	BOOST_CHECK_EQUAL(extents[10][3],3);

	BOOST_CHECK_EQUAL(extents[11][0],4);
	BOOST_CHECK_EQUAL(extents[11][1],2);
	BOOST_CHECK_EQUAL(extents[11][2],1);
	BOOST_CHECK_EQUAL(extents[11][3],3);
	BOOST_CHECK_EQUAL(extents[11][4],1);

	BOOST_CHECK_EQUAL(extents[12][0],1);
	BOOST_CHECK_EQUAL(extents[12][1],4);
	BOOST_CHECK_EQUAL(extents[12][2],2);
	BOOST_CHECK_EQUAL(extents[12][3],1);
	BOOST_CHECK_EQUAL(extents[12][4],3);
	BOOST_CHECK_EQUAL(extents[12][5],1);
}

BOOST_FIXTURE_TEST_CASE(test_extents_copy_ctor, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("copy_ctor"))
{
	BOOST_REQUIRE_EQUAL(extents.size(),13);

	auto e0  = extents[ 0]; // {}
	auto e1  = extents[ 1]; // {1,1}
	auto e2  = extents[ 2]; // {1,2}
	auto e3  = extents[ 3]; // {2,1}
	auto e4  = extents[ 4]; // {2,3}
	auto e5  = extents[ 5]; // {2,3,1}
	auto e6  = extents[ 6]; // {1,2,3}
	auto e7  = extents[ 7]; // {1,1,2,3}
	auto e8  = extents[ 8]; // {1,2,3,1,1}
	auto e9  = extents[ 9]; // {4,2,3}
	auto e10 = extents[10]; // {4,2,1,3}
	auto e11 = extents[11]; // {4,2,1,3,1}
	auto e12 = extents[12]; // {1,4,2,1,3,1}

	BOOST_CHECK_EQUAL  (e0.size(), 0);
	BOOST_CHECK (e0.empty()  );

	BOOST_REQUIRE_EQUAL(e1 .size(), 2);
	BOOST_REQUIRE_EQUAL(e2 .size(), 2);
	BOOST_REQUIRE_EQUAL(e3 .size(), 2);
	BOOST_REQUIRE_EQUAL(e4 .size(), 2);
	BOOST_REQUIRE_EQUAL(e5 .size(), 3);
	BOOST_REQUIRE_EQUAL(e6 .size(), 3);
	BOOST_REQUIRE_EQUAL(e7 .size(), 4);
	BOOST_REQUIRE_EQUAL(e8 .size(), 5);
	BOOST_REQUIRE_EQUAL(e9 .size(), 3);
	BOOST_REQUIRE_EQUAL(e10.size(), 4);
	BOOST_REQUIRE_EQUAL(e11.size(), 5);
	BOOST_REQUIRE_EQUAL(e12.size(), 6);


	BOOST_CHECK_EQUAL(e1[0],1);
	BOOST_CHECK_EQUAL(e1[1],1);

	BOOST_CHECK_EQUAL(e2[0],1);
	BOOST_CHECK_EQUAL(e2[1],2);

	BOOST_CHECK_EQUAL(e3[0],2);
	BOOST_CHECK_EQUAL(e3[1],1);

	BOOST_CHECK_EQUAL(e4[0],2);
	BOOST_CHECK_EQUAL(e4[1],3);

	BOOST_CHECK_EQUAL(e5[0],2);
	BOOST_CHECK_EQUAL(e5[1],3);
	BOOST_CHECK_EQUAL(e5[2],1);

	BOOST_CHECK_EQUAL(e6[0],1);
	BOOST_CHECK_EQUAL(e6[1],2);
	BOOST_CHECK_EQUAL(e6[2],3);

	BOOST_CHECK_EQUAL(e7[0],1);
	BOOST_CHECK_EQUAL(e7[1],1);
	BOOST_CHECK_EQUAL(e7[2],2);
	BOOST_CHECK_EQUAL(e7[3],3);

	BOOST_CHECK_EQUAL(e8[0],1);
	BOOST_CHECK_EQUAL(e8[1],2);
	BOOST_CHECK_EQUAL(e8[2],3);
	BOOST_CHECK_EQUAL(e8[3],1);
	BOOST_CHECK_EQUAL(e8[4],1);

	BOOST_CHECK_EQUAL(e9[0],4);
	BOOST_CHECK_EQUAL(e9[1],2);
	BOOST_CHECK_EQUAL(e9[2],3);

	BOOST_CHECK_EQUAL(e10[0],4);
	BOOST_CHECK_EQUAL(e10[1],2);
	BOOST_CHECK_EQUAL(e10[2],1);
	BOOST_CHECK_EQUAL(e10[3],3);

	BOOST_CHECK_EQUAL(e11[0],4);
	BOOST_CHECK_EQUAL(e11[1],2);
	BOOST_CHECK_EQUAL(e11[2],1);
	BOOST_CHECK_EQUAL(e11[3],3);
	BOOST_CHECK_EQUAL(e11[4],1);

	BOOST_CHECK_EQUAL(e12[0],1);
	BOOST_CHECK_EQUAL(e12[1],4);
	BOOST_CHECK_EQUAL(e12[2],2);
	BOOST_CHECK_EQUAL(e12[3],1);
	BOOST_CHECK_EQUAL(e12[4],3);
	BOOST_CHECK_EQUAL(e12[5],1);

}

BOOST_FIXTURE_TEST_CASE(test_extents_is, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("query"))
{
	BOOST_REQUIRE_EQUAL(extents.size(),13);

	auto e0  = extents[ 0]; // {}
	auto e1  = extents[ 1]; // {1,1}
	auto e2  = extents[ 2]; // {1,2}
	auto e3  = extents[ 3]; // {2,1}
	auto e4  = extents[ 4]; // {2,3}
	auto e5  = extents[ 5]; // {2,3,1}
	auto e6  = extents[ 6]; // {1,2,3}
	auto e7  = extents[ 7]; // {1,1,2,3}
	auto e8  = extents[ 8]; // {1,2,3,1,1}
	auto e9  = extents[ 9]; // {4,2,3}
	auto e10 = extents[10]; // {4,2,1,3}
	auto e11 = extents[11]; // {4,2,1,3,1}
	auto e12 = extents[12]; // {1,4,2,1,3,1}

	BOOST_CHECK(   e0.empty    ());
	BOOST_CHECK( ! e0.is_scalar());
	BOOST_CHECK( ! e0.is_vector());
	BOOST_CHECK( ! e0.is_matrix());
	BOOST_CHECK( ! e0.is_tensor());

	BOOST_CHECK( ! e1.empty    () );
	BOOST_CHECK(   e1.is_scalar() );
	BOOST_CHECK( ! e1.is_vector() );
	BOOST_CHECK( ! e1.is_matrix() );
	BOOST_CHECK( ! e1.is_tensor() );

	BOOST_CHECK( ! e2.empty    () );
	BOOST_CHECK( ! e2.is_scalar() );
	BOOST_CHECK(   e2.is_vector() );
	BOOST_CHECK( ! e2.is_matrix() );
	BOOST_CHECK( ! e2.is_tensor() );

	BOOST_CHECK( ! e3.empty    () );
	BOOST_CHECK( ! e3.is_scalar() );
	BOOST_CHECK(   e3.is_vector() );
	BOOST_CHECK( ! e3.is_matrix() );
	BOOST_CHECK( ! e3.is_tensor() );

	BOOST_CHECK( ! e4.empty    () );
	BOOST_CHECK( ! e4.is_scalar() );
	BOOST_CHECK( ! e4.is_vector() );
	BOOST_CHECK(   e4.is_matrix() );
	BOOST_CHECK( ! e4.is_tensor() );

	BOOST_CHECK( ! e5.empty    () );
	BOOST_CHECK( ! e5.is_scalar() );
	BOOST_CHECK( ! e5.is_vector() );
	BOOST_CHECK(   e5.is_matrix() );
	BOOST_CHECK( ! e5.is_tensor() );

	BOOST_CHECK( ! e6.empty    () );
	BOOST_CHECK( ! e6.is_scalar() );
	BOOST_CHECK( ! e6.is_vector() );
	BOOST_CHECK( ! e6.is_matrix() );
	BOOST_CHECK(   e6.is_tensor() );

	BOOST_CHECK( ! e7.empty    () );
	BOOST_CHECK( ! e7.is_scalar() );
	BOOST_CHECK( ! e7.is_vector() );
	BOOST_CHECK( ! e7.is_matrix() );
	BOOST_CHECK(   e7.is_tensor() );

	BOOST_CHECK( ! e8.empty    () );
	BOOST_CHECK( ! e8.is_scalar() );
	BOOST_CHECK( ! e8.is_vector() );
	BOOST_CHECK( ! e8.is_matrix() );
	BOOST_CHECK(   e8.is_tensor() );

	BOOST_CHECK( ! e9.empty    () );
	BOOST_CHECK( ! e9.is_scalar() );
	BOOST_CHECK( ! e9.is_vector() );
	BOOST_CHECK( ! e9.is_matrix() );
	BOOST_CHECK(   e9.is_tensor() );

	BOOST_CHECK( ! e10.empty    () );
	BOOST_CHECK( ! e10.is_scalar() );
	BOOST_CHECK( ! e10.is_vector() );
	BOOST_CHECK( ! e10.is_matrix() );
	BOOST_CHECK(   e10.is_tensor() );

	BOOST_CHECK( ! e11.empty    () );
	BOOST_CHECK( ! e11.is_scalar() );
	BOOST_CHECK( ! e11.is_vector() );
	BOOST_CHECK( ! e11.is_matrix() );
	BOOST_CHECK(   e11.is_tensor() );

	BOOST_CHECK( ! e12.empty    () );
	BOOST_CHECK( ! e12.is_scalar() );
	BOOST_CHECK( ! e12.is_vector() );
	BOOST_CHECK( ! e12.is_matrix() );
	BOOST_CHECK(   e12.is_tensor() );
}


BOOST_FIXTURE_TEST_CASE(test_extents_squeeze, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("squeeze"))
{
	BOOST_REQUIRE_EQUAL(extents.size(),13);

	auto e0  = extents[ 0].squeeze(); // {}
	auto e1  = extents[ 1].squeeze(); // {1,1}
	auto e2  = extents[ 2].squeeze(); // {1,2}
	auto e3  = extents[ 3].squeeze(); // {2,1}

	auto e4  = extents[ 4].squeeze(); // {2,3}
	auto e5  = extents[ 5].squeeze(); // {2,3}
	auto e6  = extents[ 6].squeeze(); // {2,3}
	auto e7  = extents[ 7].squeeze(); // {2,3}
	auto e8  = extents[ 8].squeeze(); // {2,3}

	auto e9  = extents[ 9].squeeze(); // {4,2,3}
	auto e10 = extents[10].squeeze(); // {4,2,3}
	auto e11 = extents[11].squeeze(); // {4,2,3}
	auto e12 = extents[12].squeeze(); // {4,2,3}

	BOOST_CHECK( (e0  == extents_type{}   ) );
	BOOST_CHECK( (e1  == extents_type{1,1}) );
	BOOST_CHECK( (e2  == extents_type{1,2}) );
	BOOST_CHECK( (e3  == extents_type{2,1}) );

	BOOST_CHECK( (e4  == extents_type{2,3}) );
	BOOST_CHECK( (e5  == extents_type{2,3}) );
	BOOST_CHECK( (e6  == extents_type{2,3}) );
	BOOST_CHECK( (e7  == extents_type{2,3}) );
	BOOST_CHECK( (e8  == extents_type{2,3}) );

	BOOST_CHECK( (e9  == extents_type{4,2,3}) );
	BOOST_CHECK( (e10 == extents_type{4,2,3}) );
	BOOST_CHECK( (e11 == extents_type{4,2,3}) );
	BOOST_CHECK( (e12 == extents_type{4,2,3}) );

}


BOOST_FIXTURE_TEST_CASE(test_extents_valid, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("valid"))
{

	using namespace boost::numeric;

	BOOST_REQUIRE_EQUAL(extents.size(),13);

	for(auto const& e : extents){
		if(e.empty())
			BOOST_CHECK_EQUAL(e.valid(),false);
		else
			BOOST_CHECK_EQUAL(e.valid(), true );
	}

	BOOST_CHECK_EQUAL( extents_type{}.valid() , false  );

	BOOST_CHECK_THROW( ublas::basic_extents<unsigned>({0,1}), std::length_error  );
	BOOST_CHECK_THROW( ublas::basic_extents<unsigned>({1,0,1}), std::length_error  );

}


BOOST_FIXTURE_TEST_CASE(test_extents_product, fixture, *boost::unit_test::label("extents") *boost::unit_test::label("product"))
{

	auto e0  = extents[ 0].product(); // {}
	auto e1  = extents[ 1].product(); // {1,1}
	auto e2  = extents[ 2].product(); // {1,2}
	auto e3  = extents[ 3].product(); // {2,1}
	auto e4  = extents[ 4].product(); // {2,3}
	auto e5  = extents[ 5].product(); // {2,3,1}
	auto e6  = extents[ 6].product(); // {1,2,3}
	auto e7  = extents[ 7].product(); // {1,1,2,3}
	auto e8  = extents[ 8].product(); // {1,2,3,1,1}
	auto e9  = extents[ 9].product(); // {4,2,3}
	auto e10 = extents[10].product(); // {4,2,1,3}
	auto e11 = extents[11].product(); // {4,2,1,3,1}
	auto e12 = extents[12].product(); // {1,4,2,1,3,1}

	BOOST_CHECK_EQUAL( e0 ,  0 );
	BOOST_CHECK_EQUAL( e1 ,  1 );
	BOOST_CHECK_EQUAL( e2 ,  2 );
	BOOST_CHECK_EQUAL( e3 ,  2 );
	BOOST_CHECK_EQUAL( e4 ,  6 );
	BOOST_CHECK_EQUAL( e5 ,  6 );
	BOOST_CHECK_EQUAL( e6 ,  6 );
	BOOST_CHECK_EQUAL( e7 ,  6 );
	BOOST_CHECK_EQUAL( e8 ,  6 );
	BOOST_CHECK_EQUAL( e9 , 24 );
	BOOST_CHECK_EQUAL( e10, 24 );
	BOOST_CHECK_EQUAL( e11, 24 );
	BOOST_CHECK_EQUAL( e12, 24 );


}

BOOST_AUTO_TEST_SUITE_END()

