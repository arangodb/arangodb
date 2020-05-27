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
#include <vector>
#include <boost/numeric/ublas/tensor/algorithms.hpp>
#include <boost/numeric/ublas/tensor/extents.hpp>
#include <boost/numeric/ublas/tensor/strides.hpp>
#include "utility.hpp"

#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_SUITE ( test_tensor_algorithms,
                        * boost::unit_test::depends_on("test_extents")
                        * boost::unit_test::depends_on("test_strides"))


using test_types  = zip<int,long,float,double,std::complex<float>>::with_t<boost::numeric::ublas::first_order, boost::numeric::ublas::last_order>;
using test_types2 = std::tuple<int,long,float,double,std::complex<float>>;

struct fixture
{
	using extents_type = boost::numeric::ublas::shape;
	fixture()
	  : extents {
	      extents_type{1,1}, // 1
	      extents_type{1,2}, // 2
	      extents_type{2,1}, // 3
	      extents_type{2,3}, // 4
	      extents_type{2,3,1}, // 5
	      extents_type{4,1,3}, // 6
	      extents_type{1,2,3}, // 7
	      extents_type{4,2,3}, // 8
	      extents_type{4,2,3,5} } // 9
	{
	}
	std::vector<extents_type> extents;
};




BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_algorithms_copy, value,  test_types2, fixture )
{
	using namespace boost::numeric;
	using value_type   = value;
	using vector_type  = std::vector<value_type>;


	for(auto const& n : extents) {

		auto a  = vector_type(n.product());
		auto b  = vector_type(n.product());
		auto c  = vector_type(n.product());

		auto wa = ublas::strides<ublas::first_order>(n);
		auto wb = ublas::strides<ublas::last_order> (n);
		auto wc = ublas::strides<ublas::first_order>(n);

		auto v = value_type{};
		for(auto i = 0ul; i < a.size(); ++i, v+=1){
			a[i]=v;
		}

		ublas::copy( n.size(), n.data(), b.data(), wb.data(), a.data(), wa.data() );
		ublas::copy( n.size(), n.data(), c.data(), wc.data(), b.data(), wb.data() );

		for(auto i = 1ul; i < c.size(); ++i)
			BOOST_CHECK_EQUAL( c[i], a[i] );

		using size_type = typename ublas::strides<ublas::first_order>::value_type;
		size_type const*const p0 = nullptr;
		BOOST_CHECK_THROW( ublas::copy( n.size(), p0, c.data(), wc.data(), b.data(), wb.data() ), std::length_error );
		BOOST_CHECK_THROW( ublas::copy( n.size(), n.data(), c.data(), p0, b.data(), wb.data() ), std::length_error );
		BOOST_CHECK_THROW( ublas::copy( n.size(), n.data(), c.data(), wc.data(), b.data(), p0 ), std::length_error );

		value_type* c0 = nullptr;
		BOOST_CHECK_THROW( ublas::copy( n.size(), n.data(), c0, wc.data(), b.data(), wb.data() ), std::length_error );
	}

	// special case rank == 0
	{
		auto n = ublas::shape{};

		auto a  = vector_type(n.product());
		auto b  = vector_type(n.product());
		auto c  = vector_type(n.product());


		auto wa = ublas::strides<ublas::first_order>(n);
		auto wb = ublas::strides<ublas::last_order> (n);
		auto wc = ublas::strides<ublas::first_order>(n);

		ublas::copy( n.size(), n.data(), b.data(), wb.data(), a.data(), wa.data() );
		ublas::copy( n.size(), n.data(), c.data(), wc.data(), b.data(), wb.data() );



		BOOST_CHECK_NO_THROW( ublas::copy( n.size(), n.data(), c.data(), wc.data(), b.data(), wb.data() ) );

	}





}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_algorithms_transform, value,  test_types2, fixture )
{
	using namespace boost::numeric;
	using value_type   = value;
	using vector_type  = std::vector<value_type>;


	for(auto const& n : extents) {

		auto a  = vector_type(n.product());
		auto b  = vector_type(n.product());
		auto c  = vector_type(n.product());

		auto wa = ublas::strides<ublas::first_order>(n);
		auto wb = ublas::strides<ublas::last_order> (n);
		auto wc = ublas::strides<ublas::first_order>(n);

		auto v = value_type{};
		for(auto i = 0ul; i < a.size(); ++i, v+=1){
			a[i]=v;
		}

		ublas::transform( n.size(), n.data(), b.data(), wb.data(), a.data(), wa.data(), [](value_type const& a){ return a + value_type(1);} );
		ublas::transform( n.size(), n.data(), c.data(), wc.data(), b.data(), wb.data(), [](value_type const& a){ return a - value_type(1);} );

		for(auto i = 1ul; i < c.size(); ++i)
			BOOST_CHECK_EQUAL( c[i], a[i] );

	}
}



BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_algorithms_accumulate, value,  test_types2, fixture )
{
	using namespace boost::numeric;
	using value_type   = value;
	using vector_type  = std::vector<value_type>;


	for(auto const& n : extents) {

		auto const s = n.product();

		auto a  = vector_type(n.product());
		//		auto b  = vector_type(n.product());
		//		auto c  = vector_type(n.product());

		auto wa = ublas::strides<ublas::first_order>(n);
		//		auto wb = ublas::strides<ublas::last_order> (n);
		//		auto wc = ublas::strides<ublas::first_order>(n);

		auto v = value_type{};
		for(auto i = 0ul; i < a.size(); ++i, v+=value_type(1)){
			a[i]=v;
		}

		auto acc = ublas::accumulate( n.size(), n.data(), a.data(), wa.data(), v);

		BOOST_CHECK_EQUAL( acc, value_type( s*(s+1) / 2 )  );


		auto acc2 = ublas::accumulate( n.size(), n.data(), a.data(), wa.data(), v,
		                               [](auto const& l, auto const& r){return l + r; });

		BOOST_CHECK_EQUAL( acc2, value_type( s*(s+1) / 2 )  );

	}
}




template<class V>
void init(std::vector<V>& a)
{
	auto v = V(1);
	for(auto i = 0u; i < a.size(); ++i, ++v){
		a[i] = v;
	}
}

template<class V>
void init(std::vector<std::complex<V>>& a)
{
	auto v = std::complex<V>(1,1);
	for(auto i = 0u; i < a.size(); ++i){
		a[i] = v;
		v.real(v.real()+1);
		v.imag(v.imag()+1);
	}
}


BOOST_FIXTURE_TEST_CASE_TEMPLATE( test_tensor_algorithms_trans, value,  test_types, fixture )
{
	using namespace boost::numeric;
	using value_type  = typename value::first_type;
	using layout_type = typename value::second_type;
	using vector_type  = std::vector<value_type>;
	using strides_type = ublas::strides<layout_type>;
	using extents_type = ublas::shape;
	using size_type = typename extents_type::value_type;
	using permutation_type = std::vector<size_type>;
	

	for(auto const& n : extents) {

		auto p   = n.size();
		auto s   = n.product();

		auto pi  = permutation_type(p);
		auto a   = vector_type(s);
		auto b1  = vector_type(s);
		auto b2  = vector_type(s);
		auto c1  = vector_type(s);
		auto c2  = vector_type(s);

		auto wa = strides_type(n);

		init(a);

		// so wie last-order.
		for(auto i = size_type(0), j = p; i < n.size(); ++i, --j)
			pi[i] = j;

		auto nc = typename extents_type::base_type (p);
		for(auto i = 0u; i < p; ++i)
			nc[pi[i]-1] = n[i];

		auto wc = strides_type(extents_type(nc));
		auto wc_pi = typename strides_type::base_type (p);
		for(auto i = 0u; i < p; ++i)
			wc_pi[pi[i]-1] = wc[i];

		ublas::copy ( p, n.data(),            c1.data(), wc_pi.data(), a.data(), wa.data());
		ublas::trans( p, n.data(), pi.data(), c2.data(), wc.data(),    a.data(), wa.data() );

		if(!std::is_compound_v<value_type>)
			for(auto i = 0ul; i < s; ++i)
				BOOST_CHECK_EQUAL( c1[i], c2[i] );


		auto nb = typename extents_type::base_type (p);
		for(auto i = 0u; i < p; ++i)
			nb[pi[i]-1] = nc[i];

		auto wb = strides_type (extents_type(nb));
		auto wb_pi = typename strides_type::base_type (p);
		for(auto i = 0u; i < p; ++i)
			wb_pi[pi[i]-1] = wb[i];

		ublas::copy ( p, nc.data(),            b1.data(), wb_pi.data(), c1.data(), wc.data());
		ublas::trans( p, nc.data(), pi.data(), b2.data(), wb.data(),    c2.data(), wc.data() );

		if(!std::is_compound_v<value_type>)
			for(auto i = 0ul; i < s; ++i)
				BOOST_CHECK_EQUAL( b1[i], b2[i] );

		for(auto i = 0ul; i < s; ++i)
			BOOST_CHECK_EQUAL( a[i], b2[i] );

	}
}


BOOST_AUTO_TEST_SUITE_END()
