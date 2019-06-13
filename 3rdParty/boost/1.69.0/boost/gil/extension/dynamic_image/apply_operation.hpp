//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_APPLY_OPERATION_HPP
#define BOOST_GIL_EXTENSION_DYNAMIC_IMAGE_APPLY_OPERATION_HPP

#include <boost/gil/extension/dynamic_image/apply_operation_base.hpp>
#include <boost/gil/extension/dynamic_image/variant.hpp>

#ifdef BOOST_GIL_DOXYGEN_ONLY
#undef BOOST_GIL_REDUCE_CODE_BLOAT
#endif

// Implements apply_operation for variants.
// Optionally performs type reduction.
#ifdef BOOST_GIL_REDUCE_CODE_BLOAT

#include <boost/gil/extension/dynamic_image/reduce.hpp>

#else

namespace boost { namespace gil {

/// \ingroup Variant
/// \brief Invokes a generic mutable operation (represented as a unary function object) on a variant
template <typename Types, typename UnaryOp> BOOST_FORCEINLINE
typename UnaryOp::result_type apply_operation(variant<Types>& arg, UnaryOp op) {
    return apply_operation_base<Types>(arg._bits, arg._index ,op);
}

/// \ingroup Variant
/// \brief Invokes a generic constant operation (represented as a unary function object) on a variant
template <typename Types, typename UnaryOp> BOOST_FORCEINLINE
typename UnaryOp::result_type apply_operation(const variant<Types>& arg, UnaryOp op) {
    return apply_operation_basec<Types>(arg._bits, arg._index ,op);
}

/// \ingroup Variant
/// \brief Invokes a generic constant operation (represented as a binary function object) on two variants
template <typename Types1, typename Types2, typename BinaryOp> BOOST_FORCEINLINE
typename BinaryOp::result_type apply_operation(const variant<Types1>& arg1, const variant<Types2>& arg2, BinaryOp op) {    
    return apply_operation_base<Types1,Types2>(arg1._bits, arg1._index, arg2._bits, arg2._index, op);
}

}}  // namespace boost::gil

#endif // defined(BOOST_GIL_REDUCE_CODE_BLOAT)

#endif
