//  (C) Copyright Gennadiy Rozental 2001.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// Defines helper routines and types for merging monomorphic samples
// ***************************************************************************

#ifndef BOOST_TEST_DATA_MONOMORPHIC_SAMPLE_MERGE_HPP
#define BOOST_TEST_DATA_MONOMORPHIC_SAMPLE_MERGE_HPP

// Boost.Test
#include <boost/test/data/config.hpp>
#include <boost/test/data/index_sequence.hpp>

#include <boost/test/data/monomorphic/fwd.hpp>

#include <boost/test/detail/suppress_warnings.hpp>

namespace boost {
namespace unit_test {
namespace data {
namespace monomorphic {

template<typename T1, typename T2>
struct merged_sample {
    typedef std::tuple<T1,T2>               type;
    typedef std::tuple<T1 const&,T2 const&> ref;
};

//____________________________________________________________________________//

template<typename T1, typename ...T>
struct merged_sample<T1, std::tuple<T...>> {
    typedef std::tuple<T1, T...>                type;
    typedef std::tuple<T1 const&, T const&...>  ref;
};

//____________________________________________________________________________//

template<typename ...T, typename T1>
struct merged_sample<std::tuple<T...>, T1> {
    typedef std::tuple<T..., T1>                type;
    typedef std::tuple<T const&..., T1 const&>  ref;
};

//____________________________________________________________________________//

template<typename ...T1, typename ...T2>
struct merged_sample<std::tuple<T1...>, std::tuple<T2...>> {
    typedef std::tuple<T1..., T2...>                type;
    typedef std::tuple<T1 const&..., T2 const&...>  ref;
};

//____________________________________________________________________________//

namespace ds_detail {

template<typename ...T1, typename ...T2, std::size_t ...I1, std::size_t ...I2>
inline typename merged_sample<std::tuple<T1...>, std::tuple<T2...>>::ref
sample_merge_impl( std::tuple<T1 const&...> const& a1, std::tuple<T2 const&...> const& a2, 
                   index_sequence<I1...> const&      , index_sequence<I2...> const& )
{
    using ref_type = typename merged_sample<std::tuple<T1...>, std::tuple<T2...>>::ref;

    return ref_type( std::get<I1>(a1)..., std::get<I2>(a2)... );
}

//____________________________________________________________________________//

template<typename ...T1, typename ...T2>
inline typename merged_sample<std::tuple<T1...>, std::tuple<T2...>>::ref
sample_merge_impl( std::tuple<T1 const&...> const& a1, std::tuple<T2 const&...> const& a2 )
{
    return sample_merge_impl( a1, a2, index_sequence_for<T1...>{}, index_sequence_for<T2...>{}  );
}

//____________________________________________________________________________//

template<typename T>
inline std::tuple<T const&>
as_tuple( T const& arg )
{
    return std::tuple<T const&>( arg );
}

//____________________________________________________________________________//

template<typename ...T>
inline std::tuple<T...> const&
as_tuple( std::tuple<T...> const& arg )
{
    return arg;
}

//____________________________________________________________________________//

} // namespace ds_detail

template<typename T1, typename T2>
inline typename merged_sample<T1, T2>::ref
sample_merge( T1 const& a1, T2 const& a2 )
{
    auto const& a1_as_tuple = ds_detail::as_tuple( a1 );
    auto const& a2_as_tuple = ds_detail::as_tuple( a2 );

    return ds_detail::sample_merge_impl( a1_as_tuple, a2_as_tuple );
}

} // namespace monomorphic
} // namespace data
} // namespace unit_test
} // namespace boost

#include <boost/test/detail/enable_warnings.hpp>

#endif // BOOST_TEST_DATA_MONOMORPHIC_SAMPLE_MERGE_HPP

