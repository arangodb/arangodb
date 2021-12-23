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
#include <algorithm>
#include <complex>
#include <boost/numeric/ublas/tensor.hpp>
#include <boost/numeric/ublas/tensor/multi_index.hpp>


#include <boost/test/unit_test.hpp>

#include "utility.hpp"


BOOST_AUTO_TEST_SUITE ( test_multi_index )


using test_types = zip<int,long,float,double,std::complex<float>>::with_t<boost::numeric::ublas::first_order, boost::numeric::ublas::last_order>;


BOOST_AUTO_TEST_CASE ( test_index_classes )
{
	using namespace boost::numeric::ublas::index;


	BOOST_CHECK_EQUAL (  _a.value  ,  1  ) ;
	BOOST_CHECK_EQUAL (  _b.value  ,  2  ) ;
	BOOST_CHECK_EQUAL (  _c.value  ,  3  ) ;
	BOOST_CHECK_EQUAL (  _d.value  ,  4  ) ;
	BOOST_CHECK_EQUAL (  _e.value  ,  5  ) ;
	BOOST_CHECK_EQUAL (  _f.value  ,  6  ) ;
	BOOST_CHECK_EQUAL (  _g.value  ,  7  ) ;
	BOOST_CHECK_EQUAL (  _h.value  ,  8  ) ;
	BOOST_CHECK_EQUAL (  _i.value  ,  9  ) ;
	BOOST_CHECK_EQUAL (  _j.value  , 10  ) ;
	BOOST_CHECK_EQUAL (  _k.value  , 11  ) ;
	BOOST_CHECK_EQUAL (  _l.value  , 12  ) ;
	BOOST_CHECK_EQUAL (  _m.value  , 13  ) ;
	BOOST_CHECK_EQUAL (  _n.value  , 14  ) ;
	BOOST_CHECK_EQUAL (  _o.value  , 15  ) ;
	BOOST_CHECK_EQUAL (  _p.value  , 16  ) ;
	BOOST_CHECK_EQUAL (  _q.value  , 17  ) ;
	BOOST_CHECK_EQUAL (  _r.value  , 18  ) ;
	BOOST_CHECK_EQUAL (  _s.value  , 19  ) ;
	BOOST_CHECK_EQUAL (  _t.value  , 20  ) ;
	BOOST_CHECK_EQUAL (  _u.value  , 21  ) ;
	BOOST_CHECK_EQUAL (  _v.value  , 22  ) ;
	BOOST_CHECK_EQUAL (  _w.value  , 23  ) ;
	BOOST_CHECK_EQUAL (  _x.value  , 24  ) ;
	BOOST_CHECK_EQUAL (  _y.value  , 25  ) ;
	BOOST_CHECK_EQUAL (  _z.value  , 26  ) ;

}

BOOST_AUTO_TEST_CASE ( test_multi_index_class_construction )
{
	using namespace boost::numeric::ublas;
	using namespace boost::numeric::ublas::index;


	{
		multi_index<2> ind(_a, _b);

		BOOST_CHECK_EQUAL ( get<0>( ind ), 1 ) ;
		BOOST_CHECK_EQUAL ( get<1>( ind ), 2 ) ;
	}


	{
		multi_index<2> ind(_d,_c);

		BOOST_CHECK_EQUAL ( ind[0] , 4 ) ;
		BOOST_CHECK_EQUAL ( ind[1] , 3 ) ;
	}
}


BOOST_AUTO_TEST_CASE_TEMPLATE( test_tensor_multi_index_class_generation, value,  test_types )
{
	using namespace boost::numeric::ublas;
	using value_type   = typename value::first_type;
	using layout_type  = typename value::second_type;
	using tensor_type  = tensor<value_type,layout_type>;

	auto t = std::make_tuple (
	      index::_a, // 0
	      index::_b, // 1
	      index::_c, // 2
	      index::_d, // 3
	      index::_e  // 4
	      );

	{
		auto a = tensor_type(shape{2,3}, value_type{2});
		auto a_ind = a( std::get<0>(t), std::get<2>(t)  );

		BOOST_CHECK_EQUAL ( std::addressof( a_ind.first ), std::addressof( a ) ) ;

		BOOST_CHECK_EQUAL (std::get<0>(a_ind.second)(), index::_a() ) ;
		BOOST_CHECK_EQUAL (std::get<1>(a_ind.second)(), index::_c() ) ;
	}

	{
		auto a = tensor_type(shape{2,3}, value_type{2});
		auto a_ind = a( std::get<2>(t), std::get<0>(t)  );

		BOOST_CHECK_EQUAL ( std::addressof( a_ind.first ), std::addressof( a ) ) ;

		BOOST_CHECK_EQUAL (std::get<0>(a_ind.second)(), index::_c() ) ;
		BOOST_CHECK_EQUAL (std::get<1>(a_ind.second)(), index::_a() ) ;
	}

	{
		auto a = tensor_type(shape{2,3}, value_type{2});
		auto a_ind = a( std::get<2>(t), std::get<3>(t)  );

		BOOST_CHECK_EQUAL (std::addressof(  a_ind.first ), std::addressof( a ) ) ;

		BOOST_CHECK_EQUAL (std::get<0>(a_ind.second)(), index::_c() ) ;
		BOOST_CHECK_EQUAL (std::get<1>(a_ind.second)(), index::_d() ) ;
	}

	{
		auto a = tensor_type(shape{2,3,4}, value_type{2});
		auto a_ind = a( std::get<2>(t), std::get<3>(t), std::get<0>(t)  );

		BOOST_CHECK_EQUAL (std::addressof(  a_ind.first ), std::addressof( a ) ) ;

		BOOST_CHECK_EQUAL (std::get<0>(a_ind.second)(), index::_c() ) ;
		BOOST_CHECK_EQUAL (std::get<1>(a_ind.second)(), index::_d() ) ;
		BOOST_CHECK_EQUAL (std::get<2>(a_ind.second)(), index::_a() ) ;
	}

}

BOOST_AUTO_TEST_SUITE_END()

