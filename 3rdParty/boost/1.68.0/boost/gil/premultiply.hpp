/*
    Copyright 2014 
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_PREMULTIPLY_HPP
#define BOOST_GIL_PREMULTIPLY_HPP

#include <iostream>

#include <boost/gil/rgba.hpp>
#include <boost/mpl/remove.hpp>

namespace boost { namespace gil {

template <typename SrcP, typename DstP>
struct channel_premultiply {
	channel_premultiply (SrcP const & src, DstP & dst)
		: src_ (src), dst_ (dst)
	{}
	
	template <typename Channel>
	void operator () (Channel c) const {
		// @todo: need to do a “channel_convert” too, in case the channel types aren't the same?
		get_color (dst_, Channel()) = channel_multiply(get_color(src_,Channel()), alpha_or_max(src_));
	}
	SrcP const & src_;
	DstP & dst_;
};


namespace detail
{
	template <typename SrcP, typename DstP>
	void assign_alpha_if(mpl::true_, SrcP const &src, DstP &dst)
	{
	  get_color (dst,alpha_t()) = alpha_or_max (src);
	};

	template <typename SrcP, typename DstP>
	void assign_alpha_if(mpl::false_, SrcP const &src, DstP &dst)
	{
	  // nothing to do
	};
}

struct premultiply {
	template <typename SrcP, typename DstP>
	void operator()(const SrcP& src, DstP& dst) const {
		typedef typename color_space_type<SrcP>::type src_colour_space_t;
		typedef typename color_space_type<DstP>::type dst_colour_space_t;
		typedef typename mpl:: remove <src_colour_space_t, alpha_t>:: type src_colour_channels;

		typedef mpl::bool_<mpl::contains<dst_colour_space_t, alpha_t>::value> has_alpha_t;
		mpl:: for_each <src_colour_channels> ( channel_premultiply <SrcP, DstP> (src, dst) );
		detail::assign_alpha_if(has_alpha_t(), src, dst);
	}
};

template <typename SrcConstRefP,  // const reference to the source pixel
          typename DstP>          // Destination pixel value (models PixelValueConcept)
class premultiply_deref_fn {
public:
    typedef premultiply_deref_fn const_t;
    typedef DstP                value_type;
    typedef value_type          reference;      // read-only dereferencing
    typedef const value_type&   const_reference;
    typedef SrcConstRefP        argument_type;
    typedef reference           result_type;
    BOOST_STATIC_CONSTANT(bool, is_mutable=false);

    result_type operator()(argument_type srcP) const {
        result_type dstP;
        premultiply () (srcP,dstP);
        return dstP;
    }
};

template <typename SrcView, typename DstP>
struct premultiplied_view_type {
private:
    typedef typename SrcView::const_t::reference src_pix_ref;  // const reference to pixel in SrcView
    typedef premultiply_deref_fn<src_pix_ref, DstP> deref_t; // the dereference adaptor that performs color conversion
    typedef typename SrcView::template add_deref<deref_t> add_ref_t;
public:
    typedef typename add_ref_t::type type; // the color converted view type
    static type make(const SrcView& sv) { return add_ref_t::make(sv, deref_t()); }
};

template <typename DstP, typename View> inline
typename premultiplied_view_type<View,DstP>::type premultiply_view(const View& src) {
    return premultiplied_view_type<View,DstP>::make(src);
}

	}
}

#endif // BOOST_GIL_PREMULTIPLY_HPP
