//  Copyright (c) 2018-2019 Cem Bassoy
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  The author gratefully acknowledge the support of
//  Fraunhofer and Google in producing this work
//  which started as a Google Summer of Code project.
//


#include <boost/numeric/ublas/tensor.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE ( test_multi_index_utility )


BOOST_AUTO_TEST_CASE ( test_multi_index_has_index )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	{
		constexpr auto tuple = std::tuple<>{};
		constexpr auto has_a = has_index<decltype(_a),decltype(tuple)>::value;
		constexpr auto has_b = has_index<decltype(_b),decltype(tuple)>::value;
		BOOST_CHECK( !has_a );
		BOOST_CHECK( !has_b );
	}


	{
		constexpr auto tuple = std::make_tuple(_a);
		constexpr auto has_a = has_index<decltype(_a),decltype(tuple)>::value;
		constexpr auto has_b = has_index<decltype(_b),decltype(tuple)>::value;
		BOOST_CHECK(  has_a );
		BOOST_CHECK( !has_b );
	}

	{
		constexpr auto tuple = std::make_tuple(_a,_b,_,_c,_d);
		constexpr auto has_a = has_index<decltype(_a),decltype(tuple)>::value;
		constexpr auto has_b = has_index<decltype(_b),decltype(tuple)>::value;
		constexpr auto has_c = has_index<decltype(_c),decltype(tuple)>::value;
		constexpr auto has_d = has_index<decltype(_d),decltype(tuple)>::value;
		constexpr auto has_e = has_index<decltype(_e),decltype(tuple)>::value;
		constexpr auto has__ = has_index<decltype( _),decltype(tuple)>::value;
		BOOST_CHECK(  has_a );
		BOOST_CHECK(  has_b );
		BOOST_CHECK(  has_c );
		BOOST_CHECK(  has_d );
		BOOST_CHECK( !has_e );
		BOOST_CHECK(  has__ );
	}
}



BOOST_AUTO_TEST_CASE ( test_multi_index_valid )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	{
		constexpr auto tuple = std::tuple<>{};
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}


	{
		constexpr auto tuple = std::make_tuple(_a);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_a,_,_b);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_a,_,_b,_b);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( !valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_c,_a,_,_b,_b);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( !valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_c,_a,_,_b);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_,_c,_a,_,_b);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}

	{
		constexpr auto tuple = std::make_tuple(_,_c,_a,_,_b,_);
		constexpr auto valid = valid_multi_index<decltype(tuple)>::value;
		BOOST_CHECK( valid );
	}
}





BOOST_AUTO_TEST_CASE ( test_multi_index_number_equal_indices )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	{
		constexpr auto lhs = std::tuple<>{};
		constexpr auto rhs = std::tuple<>{};
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::tuple<>{};
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 0 );
	}

	{
		constexpr auto lhs = std::tuple<>{};
		constexpr auto rhs = std::make_tuple(_a);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b);
		constexpr auto rhs = std::make_tuple(_a);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 0 );
	}



	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::make_tuple(_a);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_a,_b);
		constexpr auto rhs = std::make_tuple(_a);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b);
		constexpr auto rhs = std::make_tuple(_a,_b);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::make_tuple(_a);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}



	{
		constexpr auto lhs = std::make_tuple(_a,_b);
		constexpr auto rhs = std::make_tuple(_a,_b);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a);
		constexpr auto rhs = std::make_tuple(_a,_b);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_c);
		constexpr auto rhs = std::make_tuple(_a,_b);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_c);
		constexpr auto rhs = std::make_tuple(_a,_b,_d);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d);
		constexpr auto rhs = std::make_tuple(_a,_b,_d);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 3 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d);
		constexpr auto rhs = std::make_tuple(_a,_b,_d,_);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 3 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_a,_b,_d,_);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 3 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d,_);
		constexpr auto rhs = std::make_tuple( _,_b,_d,_);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_,_b,_d,_);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_,_b,_d,_,_);
		constexpr auto num = number_equal_indexes<decltype(lhs), decltype(rhs)>::value;
		BOOST_CHECK_EQUAL( num, 1 );
	}
}







BOOST_AUTO_TEST_CASE ( test_multi_index_index_position )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	{
		constexpr auto tuple = std::tuple<>{};
		constexpr auto ind   = index_position<decltype(_),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,0);
	}

	{
		constexpr auto tuple = std::make_tuple(_);
		constexpr auto ind   = index_position<decltype(_),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,0);
	}

	{
		constexpr auto tuple = std::make_tuple(_);
		constexpr auto ind   = index_position<decltype(_a),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,1);
	}

	{
		constexpr auto tuple = std::make_tuple(_,_a);
		constexpr auto ind   = index_position<decltype(_),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,0);
	}

	{
		constexpr auto tuple = std::make_tuple(_,_a);
		constexpr auto ind   = index_position<decltype(_a),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,1);
	}

	{
		constexpr auto tuple = std::make_tuple(_,_a);
		constexpr auto ind   = index_position<decltype(_b),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,2);
	}




	{
		constexpr auto tuple = std::make_tuple(_c,_,_a);
		constexpr auto ind   = index_position<decltype(_c),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,0);
	}

	{
		constexpr auto tuple = std::make_tuple(_c,_,_a,_);
		constexpr auto ind   = index_position<decltype(_),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,1);
	}

	{
		constexpr auto tuple = std::make_tuple(_c,_,_a);
		constexpr auto ind   = index_position<decltype(_a),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,2);
	}

	{
		constexpr auto tuple = std::make_tuple(_c,_,_a);
		constexpr auto ind   = index_position<decltype(_d),decltype(tuple)>::value;
		BOOST_CHECK_EQUAL(ind,3);
	}

}








BOOST_AUTO_TEST_CASE ( test_multi_index_index_position_pairs )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	{
		constexpr auto lhs   = std::tuple<>{};
		constexpr auto rhs   = std::tuple<>{};
		auto array = index_position_pairs(lhs, rhs);
		BOOST_CHECK_EQUAL(array.size(), 0ul );
	}

	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::tuple<>{};
		auto array = index_position_pairs(lhs, rhs);
		BOOST_CHECK_EQUAL(array.size(), 0ul );
	}

	{
		constexpr auto lhs = std::tuple<>{};
		constexpr auto rhs = std::make_tuple(_a);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_CHECK_EQUAL(array.size(), 0ul );
	}

	{
		constexpr auto lhs = std::make_tuple(_b);
		constexpr auto rhs = std::make_tuple(_a);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_CHECK_EQUAL(array.size(), 0ul );
	}



	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::make_tuple(_a);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_a,_b);
		constexpr auto rhs = std::make_tuple(_a);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b);
		constexpr auto rhs = std::make_tuple(_a,_b);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_a);
		constexpr auto rhs = std::make_tuple(_a);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 0 );
	}



	{
		constexpr auto lhs = std::make_tuple(_a,_b);
		constexpr auto rhs = std::make_tuple(_a,_b);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 2ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 0 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 1 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a);
		constexpr auto rhs = std::make_tuple(_a,_b);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 2ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_c);
		constexpr auto rhs = std::make_tuple(_a,_b);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 2ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_c);
		constexpr auto rhs = std::make_tuple(_a,_b,_d);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 2ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d);
		constexpr auto rhs = std::make_tuple(_a,_b,_d);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 3ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
		BOOST_CHECK_EQUAL(array[2].first , 2 );
		BOOST_CHECK_EQUAL(array[2].second, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d);
		constexpr auto rhs = std::make_tuple(_a,_b,_d,_);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 3ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
		BOOST_CHECK_EQUAL(array[2].first , 2 );
		BOOST_CHECK_EQUAL(array[2].second, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_a,_b,_d,_);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 3ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 1 );
		BOOST_CHECK_EQUAL(array[1].second, 0 );
		BOOST_CHECK_EQUAL(array[2].first , 2 );
		BOOST_CHECK_EQUAL(array[2].second, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_b,_a,_d,_);
		constexpr auto rhs = std::make_tuple( _,_b,_d,_);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 2ul );
		BOOST_CHECK_EQUAL(array[0].first , 0 );
		BOOST_CHECK_EQUAL(array[0].second, 1 );
		BOOST_CHECK_EQUAL(array[1].first , 2 );
		BOOST_CHECK_EQUAL(array[1].second, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_,_b,_d,_);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 2 );
		BOOST_CHECK_EQUAL(array[0].second, 2 );
	}

	{
		constexpr auto lhs = std::make_tuple(_,_a,_d,_);
		constexpr auto rhs = std::make_tuple(_,_b,_d,_,_);
		auto array = index_position_pairs(lhs, rhs);
		BOOST_ASSERT(array.size() == 1ul );
		BOOST_CHECK_EQUAL(array[0].first , 2 );
		BOOST_CHECK_EQUAL(array[0].second, 2 );
	}
}



BOOST_AUTO_TEST_CASE ( test_multi_index_array_to_vector )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;

	auto check = [](auto const& lhs, auto const& rhs)
	{
		auto array = index_position_pairs(lhs, rhs);

		auto vector_pair =  array_to_vector( array );

		BOOST_CHECK_EQUAL(vector_pair.first .size(), array.size() );
		BOOST_CHECK_EQUAL(vector_pair.second.size(), array.size() );

		for(auto i = 0ul; i < array.size(); ++i)
		{
			BOOST_CHECK_EQUAL(vector_pair.first [i], array[i].first +1 );
			BOOST_CHECK_EQUAL(vector_pair.second[i], array[i].second+1 );
		}

	};

	check(std::tuple<>{}        , std::tuple<>{});
	check(std::make_tuple(_a)   , std::tuple<>{});
	check(std::tuple<>{}        , std::make_tuple(_a));
	check(std::make_tuple(_a)   , std::make_tuple(_b));
	check(std::make_tuple(_a)   , std::make_tuple(_a));
	check(std::make_tuple(_a,_b), std::make_tuple(_a));
	check(std::make_tuple(_a)   , std::make_tuple(_a,_b));
	check(std::make_tuple(_a,_b), std::make_tuple(_a,_b));
	check(std::make_tuple(_b,_a), std::make_tuple(_a,_b));
	check(std::make_tuple(_b,_a,_c), std::make_tuple(_a,_b,_d));
}



BOOST_AUTO_TEST_SUITE_END()

