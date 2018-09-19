/*
    Copyright 2005-2007 Adobe Systems Incorporated
   
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_NUMERIC_ALGORITHM_HPP
#define BOOST_GIL_EXTENSION_NUMERIC_ALGORITHM_HPP

/*!
/// \file               
/// \brief Numeric algorithms
/// \author Hailin Jin and Lubomir Bourdev \n
///         Adobe Systems Incorporated
/// \date   2005-2007 \n
*/

#include <cassert>
#include <iterator>
#include <algorithm>
#include <numeric>

#include <boost/gil/gil_config.hpp>
#include <boost/gil/pixel_iterator.hpp>
#include <boost/gil/metafunctions.hpp>

namespace boost { namespace gil {

/// \brief Returns the reference proxy associated with a type that has a \p "reference" member typedef.
///
/// The reference proxy is the reference type, but with stripped-out C++ reference. It models PixelConcept
template <typename T>
struct pixel_proxy : public remove_reference<typename T::reference> {};

/// \brief std::for_each for a pair of iterators
template <typename Iterator1,typename Iterator2,typename BinaryFunction>
BinaryFunction for_each(Iterator1 first1,Iterator1 last1,Iterator2 first2,BinaryFunction f) {
    while(first1!=last1)
        f(*first1++,*first2++);
    return f;
}

template <typename SrcIterator,typename DstIterator>
inline DstIterator assign_pixels(SrcIterator src,SrcIterator src_end,DstIterator dst) {
    for_each(src,src_end,dst,pixel_assigns_t<typename pixel_proxy<typename std::iterator_traits<SrcIterator>::value_type>::type,
                                             typename pixel_proxy<typename std::iterator_traits<DstIterator>::value_type>::type>());             
    return dst+(src_end-src);
}

namespace detail {
template <std::size_t Size>
struct inner_product_k_t {
    template <class _InputIterator1, class _InputIterator2, class _Tp,
              class _BinaryOperation1, class _BinaryOperation2>
    static _Tp apply(_InputIterator1 __first1, 
                     _InputIterator2 __first2, _Tp __init, 
                     _BinaryOperation1 __binary_op1,
                     _BinaryOperation2 __binary_op2) {
        __init = __binary_op1(__init, __binary_op2(*__first1, *__first2));
        return inner_product_k_t<Size-1>::template apply(__first1+1,__first2+1,__init,
                                                         __binary_op1, __binary_op2);
    }
};

template <>
struct inner_product_k_t<0> {
    template <class _InputIterator1, class _InputIterator2, class _Tp,
              class _BinaryOperation1, class _BinaryOperation2>
    static _Tp apply(_InputIterator1 __first1, 
                     _InputIterator2 __first2, _Tp __init, 
                     _BinaryOperation1 __binary_op1,
                     _BinaryOperation2 __binary_op2) {
        return __init;
    }
};
} // namespace detail

/// static version of std::inner_product
template <std::size_t Size,
          class _InputIterator1, class _InputIterator2, class _Tp,
          class _BinaryOperation1, class _BinaryOperation2>
BOOST_FORCEINLINE
_Tp inner_product_k(_InputIterator1 __first1, 
                    _InputIterator2 __first2,
                    _Tp __init, 
                    _BinaryOperation1 __binary_op1,
                    _BinaryOperation2 __binary_op2) {
    return detail::inner_product_k_t<Size>::template apply(__first1,__first2,__init,
                                                           __binary_op1, __binary_op2);
}

/// \brief 1D un-guarded correlation with a variable-size kernel
template <typename PixelAccum,typename SrcIterator,typename KernelIterator,typename Integer,typename DstIterator>
inline DstIterator correlate_pixels_n(SrcIterator src_begin,SrcIterator src_end,
                                      KernelIterator ker_begin,Integer ker_size,
                                      DstIterator dst_begin) {
    typedef typename pixel_proxy<typename std::iterator_traits<SrcIterator>::value_type>::type PIXEL_SRC_REF;
    typedef typename pixel_proxy<typename std::iterator_traits<DstIterator>::value_type>::type PIXEL_DST_REF;
    typedef typename std::iterator_traits<KernelIterator>::value_type kernel_type;
    PixelAccum acc_zero; pixel_zeros_t<PixelAccum>()(acc_zero);
    while(src_begin!=src_end) {
        pixel_assigns_t<PixelAccum,PIXEL_DST_REF>()(
            std::inner_product(src_begin,src_begin+ker_size,ker_begin,acc_zero,
                               pixel_plus_t<PixelAccum,PixelAccum,PixelAccum>(),
                               pixel_multiplies_scalar_t<PIXEL_SRC_REF,kernel_type,PixelAccum>()),
            *dst_begin);
        ++src_begin; ++dst_begin;
    }
    return dst_begin;
}

/// \brief 1D un-guarded correlation with a fixed-size kernel
template <std::size_t Size,typename PixelAccum,typename SrcIterator,typename KernelIterator,typename DstIterator>
inline DstIterator correlate_pixels_k(SrcIterator src_begin,SrcIterator src_end,
                                      KernelIterator ker_begin,
                                      DstIterator dst_begin) {
    typedef typename pixel_proxy<typename std::iterator_traits<SrcIterator>::value_type>::type PIXEL_SRC_REF;
    typedef typename pixel_proxy<typename std::iterator_traits<DstIterator>::value_type>::type PIXEL_DST_REF;
    typedef typename std::iterator_traits<KernelIterator>::value_type kernel_type;
    PixelAccum acc_zero; pixel_zeros_t<PixelAccum>()(acc_zero);
    while(src_begin!=src_end) {
        pixel_assigns_t<PixelAccum,PIXEL_DST_REF>()(
            inner_product_k<Size>(src_begin,ker_begin,acc_zero,
                                  pixel_plus_t<PixelAccum,PixelAccum,PixelAccum>(),
                                  pixel_multiplies_scalar_t<PIXEL_SRC_REF,kernel_type,PixelAccum>()),
            *dst_begin);
        ++src_begin; ++dst_begin;
    }
    return dst_begin;
}

/// \brief destination is set to be product of the source and a scalar
template <typename PixelAccum,typename SrcView,typename Scalar,typename DstView>
inline void view_multiplies_scalar(const SrcView& src,const Scalar& scalar,const DstView& dst) {
    assert(src.dimensions()==dst.dimensions());
    typedef typename pixel_proxy<typename SrcView::value_type>::type PIXEL_SRC_REF;
    typedef typename pixel_proxy<typename DstView::value_type>::type PIXEL_DST_REF;
    int height=src.height();
    for(int rr=0;rr<height;++rr) {
        typename SrcView::x_iterator it_src=src.row_begin(rr);
        typename DstView::x_iterator it_dst=dst.row_begin(rr);
        typename SrcView::x_iterator it_src_end=src.row_end(rr);
        while(it_src!=it_src_end) {
            pixel_assigns_t<PixelAccum,PIXEL_DST_REF>()(
                pixel_multiplies_scalar_t<PIXEL_SRC_REF,Scalar,PixelAccum>()(*it_src,scalar),
                *it_dst);
            ++it_src; ++it_dst;
        }
    }
}

} }  // namespace boost::gil

#endif // BOOST_GIL_EXTENSION_NUMERIC_ALGORITHM_HPP
