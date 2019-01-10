//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_UTILITIES_HPP
#define BOOST_GIL_UTILITIES_HPP

#include <boost/mpl/begin.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/size.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <utility>

namespace boost { namespace gil {

/// Various utilities not specific to the image library.
/// Some are non-standard STL extensions or generic iterator adaptors

////////////////////////////////////////////////////////////////////////////////
///  Rounding of real numbers / points to integers / integer points
////////////////////////////////////////////////////////////////////////////////

inline std::ptrdiff_t iround(float x)
{
    return static_cast<std::ptrdiff_t>(x + (x < 0.0f ? -0.5f : 0.5f));
}

inline std::ptrdiff_t iround(double x)
{
    return static_cast<std::ptrdiff_t>(x + (x < 0.0 ? -0.5 : 0.5));
}

inline std::ptrdiff_t ifloor(float x)
{
    return static_cast<std::ptrdiff_t>(std::floor(x));
}

inline std::ptrdiff_t ifloor(double x)
{
    return static_cast<std::ptrdiff_t>(std::floor(x));
}

inline std::ptrdiff_t iceil(float x)
{
    return static_cast<std::ptrdiff_t>(std::ceil(x));
}

inline std::ptrdiff_t iceil(double x)
{
    return static_cast<std::ptrdiff_t>(std::ceil(x));
}

////////////////////////////////////////////////////////////////////////////////
///  computing size with alignment
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline T align(T val, std::size_t alignment)
{
    return val+(alignment - val%alignment)%alignment;
}

/// \brief Helper base class for pixel dereference adaptors.
/// \ingroup PixelDereferenceAdaptorModel
///
template
<
    typename ConstT,
    typename Value,
    typename Reference,
    typename ConstReference,
    typename ArgType,
    typename ResultType,
    bool IsMutable
>
struct deref_base
{
    typedef ArgType        argument_type;
    typedef ResultType     result_type;
    typedef ConstT         const_t;
    typedef Value          value_type;
    typedef Reference      reference;
    typedef ConstReference const_reference;
    BOOST_STATIC_CONSTANT(bool, is_mutable = IsMutable);
};

/// \brief Composes two dereference function objects. Similar to std::unary_compose but needs to pull some typedefs from the component types.  Models: PixelDereferenceAdaptorConcept
/// \ingroup PixelDereferenceAdaptorModel
///
template <typename D1, typename D2>
class deref_compose : public deref_base
<
      deref_compose<typename D1::const_t, typename D2::const_t>,
      typename D1::value_type,
      typename D1::reference,
      typename D1::const_reference,
      typename D2::argument_type,
      typename D1::result_type,
      D1::is_mutable && D2::is_mutable
>
{
public:
    D1 _fn1;
    D2 _fn2;

    typedef typename D2::argument_type   argument_type;
    typedef typename D1::result_type     result_type;

    deref_compose() = default;
    deref_compose(const D1& x, const D2& y) : _fn1(x), _fn2(y) {}
    deref_compose(const deref_compose& dc)  : _fn1(dc._fn1), _fn2(dc._fn2) {}

    template <typename _D1, typename _D2>
    deref_compose(const deref_compose<_D1,_D2>& dc)
        : _fn1(dc._fn1), _fn2(dc._fn2)
    {}

    result_type operator()(argument_type x) const { return _fn1(_fn2(x)); }
    result_type operator()(argument_type x)       { return _fn1(_fn2(x)); }
};

// reinterpret_cast is implementation-defined. Static cast is not.
template <typename OutPtr, typename In>
BOOST_FORCEINLINE
OutPtr gil_reinterpret_cast(In* p)
{
    return static_cast<OutPtr>(static_cast<void*>(p));
}

template <typename OutPtr, typename In> BOOST_FORCEINLINE
const OutPtr gil_reinterpret_cast_c(const In* p)
{
    return static_cast<const OutPtr>(static_cast<const void*>(p));
}

namespace detail {

////////////////////////////////////////////////////////////////////////////////
///  \brief copy_n taken from SGI STL.
////////////////////////////////////////////////////////////////////////////////

template <class InputIter, class Size, class OutputIter>
std::pair<InputIter, OutputIter> _copy_n(InputIter first, Size count,
    OutputIter result, std::input_iterator_tag)
{
   for ( ; count > 0; --count)
   {
      *result = *first;
      ++first;
      ++result;
   }
   return std::pair<InputIter, OutputIter>(first, result);
}

template <class RAIter, class Size, class OutputIter>
inline std::pair<RAIter, OutputIter>
_copy_n(RAIter first, Size count, OutputIter result, std::random_access_iterator_tag)
{
   RAIter last = first + count;
   return std::pair<RAIter, OutputIter>(last, std::copy(first, last, result));
}

template <class InputIter, class Size, class OutputIter>
inline std::pair<InputIter, OutputIter>
_copy_n(InputIter first, Size count, OutputIter result)
{
   return _copy_n(first, count, result, typename std::iterator_traits<InputIter>::iterator_category());
}

template <class InputIter, class Size, class OutputIter>
inline std::pair<InputIter, OutputIter>
copy_n(InputIter first, Size count, OutputIter result)
{
    return detail::_copy_n(first, count, result);
}

/// \brief identity taken from SGI STL.
template <typename T>
struct identity
{
    typedef T argument_type;
    typedef T result_type;
    const T& operator()(const T& val) const { return val; }
};

/// \brief plus function object whose arguments may be of different type.
template <typename T1, typename T2>
struct plus_asymmetric {
    typedef T1 first_argument_type;
    typedef T2 second_argument_type;
    typedef T1 result_type;
    T1 operator()(T1 f1, T2 f2) const
    {
        return f1+f2;
    }
};

/// \brief operator++ wrapped in a function object
template <typename T>
struct inc
{
    typedef T argument_type;
    typedef T result_type;
    T operator()(T x) const { return ++x; }
};

/// \brief operator-- wrapped in a function object
template <typename T>
struct dec
{
    typedef T argument_type;
    typedef T result_type;
    T operator()(T x) const { return --x; }
};

/// \brief Returns the index corresponding to the first occurrance of a given given type in
//         a given MPL RandomAccessSequence (or size if the type is not present)
template <typename Types, typename T>
struct type_to_index
    : public mpl::distance
        <
            typename mpl::begin<Types>::type,
            typename mpl::find<Types,T>::type
        >::type
    {
    };
} // namespace detail

/// \ingroup ColorSpaceAndLayoutModel
/// \brief Represents a color space and ordering of channels in memory
template <typename ColorSpace, typename ChannelMapping = mpl::range_c<int,0,mpl::size<ColorSpace>::value>>
struct layout
{
    typedef ColorSpace      color_space_t;
    typedef ChannelMapping  channel_mapping_t;
};

/// \brief A version of swap that also works with reference proxy objects
template <typename Value, typename T1, typename T2> // where value_type<T1>  == value_type<T2> == Value
void swap_proxy(T1& left, T2& right)
{
    Value tmp = left;
    left = right;
    right = tmp;
}

/// \brief Run-time detection of whether the underlying architecture is little endian
BOOST_FORCEINLINE bool little_endian()
{
    short tester = 0x0001;
    return  *(char*)&tester!=0;
}
/// \brief Run-time detection of whether the underlying architecture is big endian
BOOST_FORCEINLINE bool big_endian()
{
    return !little_endian();
}

}}  // namespace boost::gil

#endif
