//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_NUMERIC_CONVOLVE_HPP
#define BOOST_GIL_EXTENSION_NUMERIC_CONVOLVE_HPP

#include <boost/gil/extension/numeric/algorithm.hpp>
#include <boost/gil/extension/numeric/kernel.hpp>
#include <boost/gil/extension/numeric/pixel_numeric_operations.hpp>

#include <boost/gil/algorithm.hpp>
#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/metafunctions.hpp>

#include <boost/assert.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <vector>

namespace boost { namespace gil {

// 2D seperable convolutions and correlations

/// \ingroup ImageAlgorithms
/// Boundary options for 1D correlations/convolutions
enum convolve_boundary_option  {
    convolve_option_output_ignore,  /// do nothing to the output
    convolve_option_output_zero,    /// set the output to zero
    convolve_option_extend_padded,  /// assume the source boundaries to be padded already
    convolve_option_extend_zero,    /// assume the source boundaries to be zero
    convolve_option_extend_constant /// assume the source boundaries to be the boundary value
};

namespace detail {
/// compute the correlation of 1D kernel with the rows of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView,typename Correlator>
void correlate_rows_imp(const SrcView& src, const Kernel& ker, const DstView& dst,
                        convolve_boundary_option option,
                        Correlator correlator) {
    BOOST_ASSERT(src.dimensions() == dst.dimensions());
    BOOST_ASSERT(ker.size() != 0);

    using PIXEL_SRC_REF = typename pixel_proxy<typename SrcView::value_type>::type;
    using PIXEL_DST_REF = typename pixel_proxy<typename DstView::value_type>::type;

    if(ker.size()==1) {//reduces to a multiplication
        view_multiplies_scalar<PixelAccum>(src,*ker.begin(),dst);
        return;
    }

    int width=src.width(),height=src.height();
    PixelAccum acc_zero; pixel_zeros_t<PixelAccum>()(acc_zero);
    if (width==0) return;
    if (option==convolve_option_output_ignore || option==convolve_option_output_zero) {
        typename DstView::value_type dst_zero; pixel_assigns_t<PixelAccum,PIXEL_DST_REF>()(acc_zero,dst_zero);
        if (width<(int)ker.size()) {
            if (option==convolve_option_output_zero)
                fill_pixels(dst,dst_zero);
        } else {
            std::vector<PixelAccum> buffer(width);
            for(int rr=0;rr<height;++rr) {
                assign_pixels(src.row_begin(rr),src.row_end(rr),&buffer.front());
                typename DstView::x_iterator it_dst=dst.row_begin(rr);
                if (option==convolve_option_output_zero)
                    std::fill_n(it_dst,ker.left_size(),dst_zero);
                it_dst+=ker.left_size();
                correlator(&buffer.front(),&buffer.front()+width+1-ker.size(),
                           ker.begin(),it_dst);
                it_dst+=width+1-ker.size();
                if (option==convolve_option_output_zero)
                    std::fill_n(it_dst,ker.right_size(),dst_zero);
            }
        }
    } else {
        std::vector<PixelAccum> buffer(width+ker.size()-1);
        for(int rr=0;rr<height;++rr) {
            PixelAccum* it_buffer=&buffer.front();
            if        (option==convolve_option_extend_padded) {
                assign_pixels(src.row_begin(rr)-ker.left_size(),
                              src.row_end(rr)+ker.right_size(),
                              it_buffer);
            } else if (option==convolve_option_extend_zero) {
                std::fill_n(it_buffer,ker.left_size(),acc_zero);
                it_buffer+=ker.left_size();
                assign_pixels(src.row_begin(rr),src.row_end(rr),it_buffer);
                it_buffer+=width;
                std::fill_n(it_buffer,ker.right_size(),acc_zero);
            } else if (option==convolve_option_extend_constant) {
                PixelAccum filler;
                pixel_assigns_t<PIXEL_SRC_REF,PixelAccum>()(*src.row_begin(rr),filler);
                std::fill_n(it_buffer,ker.left_size(),filler);
                it_buffer+=ker.left_size();
                assign_pixels(src.row_begin(rr),src.row_end(rr),it_buffer);
                it_buffer+=width;
                pixel_assigns_t<PIXEL_SRC_REF,PixelAccum>()(src.row_end(rr)[-1],filler);
                std::fill_n(it_buffer,ker.right_size(),filler);
            }
            correlator(&buffer.front(),&buffer.front()+width,
                       ker.begin(),
                       dst.row_begin(rr));
        }
    }
}
template <typename PixelAccum>
class correlator_n {
private:
    std::size_t _size;
public:
    correlator_n(std::size_t size_in) : _size(size_in) {}
    template <typename SrcIterator,typename KernelIterator,typename DstIterator>
    void operator()(SrcIterator src_begin,SrcIterator src_end,
                    KernelIterator ker_begin,
                    DstIterator dst_begin) {
        correlate_pixels_n<PixelAccum>(src_begin,src_end,ker_begin,_size,dst_begin);
    }
};
template <std::size_t Size,typename PixelAccum>
struct correlator_k {
public:
    template <typename SrcIterator,typename KernelIterator,typename DstIterator>
    void operator()(SrcIterator src_begin,SrcIterator src_end,
                    KernelIterator ker_begin,
                    DstIterator dst_begin){
        correlate_pixels_k<Size,PixelAccum>(src_begin,src_end,ker_begin,dst_begin);
    }
};
} // namespace detail

/// \ingroup ImageAlgorithms
///correlate a 1D variable-size kernel along the rows of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void correlate_rows(const SrcView& src, const Kernel& ker, const DstView& dst,
                    convolve_boundary_option option=convolve_option_extend_zero) {
    detail::correlate_rows_imp<PixelAccum>(src,ker,dst,option,detail::correlator_n<PixelAccum>(ker.size()));
}

/// \ingroup ImageAlgorithms
///correlate a 1D variable-size kernel along the columns of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void correlate_cols(const SrcView& src, const Kernel& ker, const DstView& dst,
                    convolve_boundary_option option=convolve_option_extend_zero) {
    correlate_rows<PixelAccum>(transposed_view(src),ker,transposed_view(dst),option);
}

/// \ingroup ImageAlgorithms
///convolve a 1D variable-size kernel along the rows of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void convolve_rows(const SrcView& src, const Kernel& ker, const DstView& dst,
                   convolve_boundary_option option=convolve_option_extend_zero) {
    correlate_rows<PixelAccum>(src,reverse_kernel(ker),dst,option);
}

/// \ingroup ImageAlgorithms
///convolve a 1D variable-size kernel along the columns of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void convolve_cols(const SrcView& src, const Kernel& ker, const DstView& dst,
                   convolve_boundary_option option=convolve_option_extend_zero) {
    convolve_rows<PixelAccum>(transposed_view(src),ker,transposed_view(dst),option);
}

/// \ingroup ImageAlgorithms
///correlate a 1D fixed-size kernel along the rows of an image
template <typename PixelAccum, typename SrcView, typename Kernel, typename DstView>
BOOST_FORCEINLINE
void correlate_rows_fixed(const SrcView& src, const Kernel& kernel, const DstView& dst,
                          convolve_boundary_option option=convolve_option_extend_zero)
{
    using correlator = detail::correlator_k
        <
            std::extent<Kernel>::value,
            PixelAccum
        >;
    detail::correlate_rows_imp<PixelAccum>(
        src, kernel, dst, option, correlator{});
}

/// \ingroup ImageAlgorithms
///correlate a 1D fixed-size kernel along the columns of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void correlate_cols_fixed(const SrcView& src, const Kernel& ker, const DstView& dst,
                          convolve_boundary_option option=convolve_option_extend_zero) {
    correlate_rows_fixed<PixelAccum>(transposed_view(src),ker,transposed_view(dst),option);
}

/// \ingroup ImageAlgorithms
///convolve a 1D fixed-size kernel along the rows of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void convolve_rows_fixed(const SrcView& src, const Kernel& ker, const DstView& dst,
                         convolve_boundary_option option=convolve_option_extend_zero) {
    correlate_rows_fixed<PixelAccum>(src,reverse_kernel(ker),dst,option);
}

/// \ingroup ImageAlgorithms
///convolve a 1D fixed-size kernel along the columns of an image
template <typename PixelAccum,typename SrcView,typename Kernel,typename DstView>
BOOST_FORCEINLINE
void convolve_cols_fixed(const SrcView& src, const Kernel& ker, const DstView& dst,
                         convolve_boundary_option option=convolve_option_extend_zero) {
    convolve_rows_fixed<PixelAccum>(transposed_view(src),ker,transposed_view(dst),option);
}

}} // namespace boost::gil

#endif
