//  boost/endian/conversion.hpp  -------------------------------------------------------//

//  Copyright Beman Dawes 2010, 2011, 2014

//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_ENDIAN_CONVERSION_HPP
#define BOOST_ENDIAN_CONVERSION_HPP

#include <boost/endian/detail/endian_reverse.hpp>
#include <boost/endian/detail/endian_load.hpp>
#include <boost/endian/detail/endian_store.hpp>
#include <boost/endian/detail/order.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/predef/other/endian.h>
#include <boost/static_assert.hpp>
#include <boost/cstdint.hpp>
#include <boost/config.hpp>

//------------------------------------- synopsis ---------------------------------------//

namespace boost
{
namespace endian
{

//--------------------------------------------------------------------------------------//
//                                                                                      //
//                             return-by-value interfaces                               //
//                             suggested by Phil Endecott                               //
//                                                                                      //
//                             user-defined types (UDTs)                                //
//                                                                                      //
//  All return-by-value conversion function templates are required to be implemented in //
//  terms of an unqualified call to "endian_reverse(x)", a function returning the       //
//  value of x with endianness reversed. This provides a customization point for any    //
//  UDT that provides a "endian_reverse" free-function meeting the requirements.        //
//  It must be defined in the same namespace as the UDT itself so that it will be found //
//  by argument dependent lookup (ADL).                                                 //
//                                                                                      //
//--------------------------------------------------------------------------------------//

  //  reverse byte order
  //  requires T to be a non-bool integral type
  //  in detail/endian_reverse.hpp
  template<class T> inline T endian_reverse( T x ) BOOST_NOEXCEPT;

  //  reverse byte order unless native endianness is big
  template <class EndianReversible >
    inline EndianReversible  big_to_native(EndianReversible  x) BOOST_NOEXCEPT;
    //  Returns: x if native endian order is big, otherwise endian_reverse(x)
  template <class EndianReversible >
    inline EndianReversible  native_to_big(EndianReversible  x) BOOST_NOEXCEPT;
    //  Returns: x if native endian order is big, otherwise endian_reverse(x)

  //  reverse byte order unless native endianness is little
  template <class EndianReversible >
    inline EndianReversible  little_to_native(EndianReversible  x) BOOST_NOEXCEPT;
    //  Returns: x if native endian order is little, otherwise endian_reverse(x)
  template <class EndianReversible >
    inline EndianReversible  native_to_little(EndianReversible  x) BOOST_NOEXCEPT;
    //  Returns: x if native endian order is little, otherwise endian_reverse(x)

  //  generic conditional reverse byte order
  template <BOOST_SCOPED_ENUM(order) From, BOOST_SCOPED_ENUM(order) To,
    class EndianReversible>
      inline EndianReversible  conditional_reverse(EndianReversible from) BOOST_NOEXCEPT;
    //  Returns: If From == To have different values, from.
    //           Otherwise endian_reverse(from).
    //  Remarks: The From == To test, and as a consequence which form the return takes, is
    //           is determined at compile time.

  //  runtime conditional reverse byte order
  template <class EndianReversible >
    inline EndianReversible  conditional_reverse(EndianReversible from,
      BOOST_SCOPED_ENUM(order) from_order, BOOST_SCOPED_ENUM(order) to_order)
        BOOST_NOEXCEPT;
      //  Returns: from_order == to_order ? from : endian_reverse(from).

  //------------------------------------------------------------------------------------//


  //  Q: What happened to bswap, htobe, and the other synonym functions based on names
  //     popularized by BSD, OS X, and Linux?
  //  A: Turned out these may be implemented as macros on some systems. Ditto POSIX names
  //     for such functionality. Since macros would cause endless problems with functions
  //     of the same names, and these functions are just synonyms anyhow, they have been
  //     removed.


  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                            reverse in place interfaces                             //
  //                                                                                    //
  //                             user-defined types (UDTs)                              //
  //                                                                                    //
  //  All reverse in place function templates are required to be implemented in terms   //
  //  of an unqualified call to "endian_reverse_inplace(x)", a function reversing       //
  //  the endianness of x, which is a non-const reference. This provides a              //
  //  customization point for any UDT that provides a "reverse_inplace" free-function   //
  //  meeting the requirements. The free-function must be declared in the same          //
  //  namespace as the UDT itself so that it will be found by argument-dependent        //
  //   lookup (ADL).                                                                    //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

  //  reverse in place
  //  in detail/endian_reverse.hpp
  template <class EndianReversible>
    inline void endian_reverse_inplace(EndianReversible& x) BOOST_NOEXCEPT;
    //  Effects: x = endian_reverse(x)

  //  reverse in place unless native endianness is big
  template <class EndianReversibleInplace>
    inline void big_to_native_inplace(EndianReversibleInplace& x) BOOST_NOEXCEPT;
    //  Effects: none if native byte-order is big, otherwise endian_reverse_inplace(x)
  template <class EndianReversibleInplace>
    inline void native_to_big_inplace(EndianReversibleInplace& x) BOOST_NOEXCEPT;
    //  Effects: none if native byte-order is big, otherwise endian_reverse_inplace(x)

  //  reverse in place unless native endianness is little
  template <class EndianReversibleInplace>
    inline void little_to_native_inplace(EndianReversibleInplace& x) BOOST_NOEXCEPT;
    //  Effects: none if native byte-order is little, otherwise endian_reverse_inplace(x);
  template <class EndianReversibleInplace>
    inline void native_to_little_inplace(EndianReversibleInplace& x) BOOST_NOEXCEPT;
    //  Effects: none if native byte-order is little, otherwise endian_reverse_inplace(x);

  //  generic conditional reverse in place
  template <BOOST_SCOPED_ENUM(order) From, BOOST_SCOPED_ENUM(order) To,
    class EndianReversibleInplace>
  inline void conditional_reverse_inplace(EndianReversibleInplace& x) BOOST_NOEXCEPT;

  //  runtime reverse in place
  template <class EndianReversibleInplace>
  inline void conditional_reverse_inplace(EndianReversibleInplace& x,
    BOOST_SCOPED_ENUM(order) from_order,  BOOST_SCOPED_ENUM(order) to_order)
    BOOST_NOEXCEPT;

//----------------------------------- end synopsis -------------------------------------//

namespace detail
{

template<class T> struct is_endian_reversible: boost::integral_constant<bool,
    boost::is_class<T>::value || ( boost::is_integral<T>::value && !boost::is_same<T, bool>::value )>
{
};

} // namespace detail

template <class EndianReversible>
inline EndianReversible big_to_native( EndianReversible x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );

#if BOOST_ENDIAN_BIG_BYTE

    return x;

#else

    return endian_reverse(x);

#endif
  }

template <class EndianReversible>
inline EndianReversible native_to_big( EndianReversible x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );

#if BOOST_ENDIAN_BIG_BYTE

    return x;

#else

    return endian_reverse(x);

#endif
}

template <class EndianReversible>
inline EndianReversible little_to_native( EndianReversible x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );

#if BOOST_ENDIAN_LITTLE_BYTE

    return x;

#else

    return endian_reverse(x);

#endif
}

template <class EndianReversible>
inline EndianReversible native_to_little( EndianReversible x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );

#if BOOST_ENDIAN_LITTLE_BYTE

    return x;

#else

    return endian_reverse(x);

#endif
}

namespace detail
{

template<class EndianReversible>
inline EndianReversible conditional_reverse_impl( EndianReversible x, boost::true_type ) BOOST_NOEXCEPT
{
    return x;
}

template<class EndianReversible>
inline EndianReversible conditional_reverse_impl( EndianReversible x, boost::false_type ) BOOST_NOEXCEPT
{
    return endian_reverse( x );
}

} // namespace detail

// generic conditional reverse
template <BOOST_SCOPED_ENUM(order) From, BOOST_SCOPED_ENUM(order) To, class EndianReversible>
inline EndianReversible conditional_reverse( EndianReversible x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );
    return detail::conditional_reverse_impl( x, boost::integral_constant<bool, From == To>() );
}

// runtime conditional reverse
template <class EndianReversible>
inline EndianReversible conditional_reverse( EndianReversible x,
    BOOST_SCOPED_ENUM(order) from_order, BOOST_SCOPED_ENUM(order) to_order ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible<EndianReversible>::value );
    return from_order == to_order? x: endian_reverse( x );
}

//--------------------------------------------------------------------------------------//
//                           reverse-in-place implementation                            //
//--------------------------------------------------------------------------------------//

namespace detail
{

template<class T> struct is_endian_reversible_inplace: boost::integral_constant<bool,
    boost::is_class<T>::value || ( boost::is_integral<T>::value && !boost::is_same<T, bool>::value )>
{
};

} // namespace detail

#if BOOST_ENDIAN_BIG_BYTE

template <class EndianReversibleInplace>
inline void big_to_native_inplace( EndianReversibleInplace& ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
}

#else

template <class EndianReversibleInplace>
inline void big_to_native_inplace( EndianReversibleInplace& x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
    endian_reverse_inplace( x );
}

#endif

#if BOOST_ENDIAN_BIG_BYTE

template <class EndianReversibleInplace>
inline void native_to_big_inplace( EndianReversibleInplace& ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
}

#else

template <class EndianReversibleInplace>
inline void native_to_big_inplace( EndianReversibleInplace& x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
    endian_reverse_inplace( x );
}

#endif

#if BOOST_ENDIAN_LITTLE_BYTE

template <class EndianReversibleInplace>
inline void little_to_native_inplace( EndianReversibleInplace& ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
}

#else

template <class EndianReversibleInplace>
inline void little_to_native_inplace( EndianReversibleInplace& x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
    endian_reverse_inplace( x );
}

#endif

#if BOOST_ENDIAN_LITTLE_BYTE

template <class EndianReversibleInplace>
inline void native_to_little_inplace( EndianReversibleInplace& ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
}

#else

template <class EndianReversibleInplace>
inline void native_to_little_inplace( EndianReversibleInplace& x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
    endian_reverse_inplace( x );
}

#endif

namespace detail
{

template<class EndianReversibleInplace>
inline void conditional_reverse_inplace_impl( EndianReversibleInplace&, boost::true_type ) BOOST_NOEXCEPT
{
}

template<class EndianReversibleInplace>
inline void conditional_reverse_inplace_impl( EndianReversibleInplace& x, boost::false_type ) BOOST_NOEXCEPT
{
    endian_reverse_inplace( x );
}

}  // namespace detail

// generic conditional reverse in place
template <BOOST_SCOPED_ENUM(order) From, BOOST_SCOPED_ENUM(order) To, class EndianReversibleInplace>
inline void conditional_reverse_inplace( EndianReversibleInplace& x ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );
    detail::conditional_reverse_inplace_impl( x, boost::integral_constant<bool, From == To>() );
}

// runtime reverse in place
template <class EndianReversibleInplace>
inline void conditional_reverse_inplace( EndianReversibleInplace& x,
    BOOST_SCOPED_ENUM(order) from_order, BOOST_SCOPED_ENUM(order) to_order ) BOOST_NOEXCEPT
{
    BOOST_STATIC_ASSERT( detail::is_endian_reversible_inplace<EndianReversibleInplace>::value );

    if( from_order != to_order )
    {
        endian_reverse_inplace( x );
    }
}

}  // namespace endian
}  // namespace boost

#endif // BOOST_ENDIAN_CONVERSION_HPP
