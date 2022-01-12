// Boost.Container
//
// varray details
//
// Copyright (c) 2012-2013 Adam Wulkiewicz, Lodz, Poland.
// Copyright (c) 2011-2013 Andrew Hundt.
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTAINER_DETAIL_VARRAY_UTIL_HPP
#define BOOST_CONTAINER_DETAIL_VARRAY_UTIL_HPP

#include <cstddef>
#include <cstring>
#include <memory>
#include <limits>

#include <boost/config.hpp>
#include <boost/core/no_exceptions_support.hpp>

#include <boost/container/detail/addressof.hpp>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
#include <boost/container/detail/iterator.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>

#include <boost/move/algorithm.hpp>
#include <boost/move/traits.hpp>
#include <boost/move/utility_core.hpp>

// TODO - move vectors iterators optimization to the other, optional file instead of checking defines?

#if defined(BOOST_CONTAINER_VARRAY_ENABLE_VECTORS_OPTIMIZATION) && !defined(BOOST_NO_EXCEPTIONS)
#include <vector>
#include <boost/container/vector.hpp>
#endif // BOOST_CONTAINER_VARRAY_ENABLE_ITERATORS_OPTIMIZATION && !BOOST_NO_EXCEPTIONS

namespace boost { namespace container { namespace varray_detail {

namespace bcd = ::boost::container::dtl;

template <typename I>
struct are_elements_contiguous : boost::container::dtl::is_pointer<I>
{};

#if defined(BOOST_CONTAINER_VARRAY_ENABLE_VECTORS_OPTIMIZATION) && !defined(BOOST_NO_EXCEPTIONS)

template <typename Pointer>
struct are_elements_contiguous<
    bcd::vector_const_iterator<Pointer>
> : bcd::true_type
{};

template <typename Pointer>
struct are_elements_contiguous<
    bcd::vector_iterator<Pointer>
> : bcd::true_type
{};

#if defined(BOOST_DINKUMWARE_STDLIB)

template <typename T>
struct are_elements_contiguous<
    std::_Vector_const_iterator<T>
> : bcd::true_type
{};

template <typename T>
struct are_elements_contiguous<
    std::_Vector_iterator<T>
> : bcd::true_type
{};

#elif defined(BOOST_GNU_STDLIB)

template <typename P, typename T, typename Allocator>
struct are_elements_contiguous<
    __gnu_cxx::__normal_iterator<P, std::vector<T, Allocator> >
> : bcd::true_type
{};

#elif defined(_LIBCPP_VERSION)

// TODO - test it first
//template <typename P>
//struct are_elements_contiguous<
//    __wrap_iter<P>
//> : bcd::true_type
//{};

#else // OTHER_STDLIB

// TODO - add other iterators implementations

#endif // STDLIB

#endif // BOOST_CONTAINER_VARRAY_ENABLE_VECTORS_OPTIMIZATION && !BOOST_NO_EXCEPTIONS

template <typename I, typename O>
struct are_corresponding :
    bcd::bool_<
        bcd::is_same<
            bcd::remove_const<
                typename ::boost::container::iterator_traits<I>::value_type
            >,
            bcd::remove_const<
                typename ::boost::container::iterator_traits<O>::value_type
            >
        >::value &&
        are_elements_contiguous<I>::value &&
        are_elements_contiguous<O>::value
    >
{};

template <typename I, typename V>
struct is_corresponding_value :
   bcd::bool_<
         bcd::is_same<
            bcd::remove_const<typename ::boost::container::iterator_traits<I>::value_type>,
            bcd::remove_const<V>
         >::value
      >
{};

// destroy(I, I)

template <typename I>
void destroy_dispatch(I /*first*/, I /*last*/, bcd::true_type const& /*is_trivially_destructible*/)
{}

template <typename I>
void destroy_dispatch(I first, I last, bcd::false_type const& /*is_trivially_destructible*/)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    for ( ; first != last ; ++first )
        first->~value_type();
}

template <typename I>
void destroy(I first, I last)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    destroy_dispatch(first, last, bcd::bool_<bcd::is_trivially_destructible<value_type>::value>());
}

// destroy(I)

template <typename I>
void destroy_dispatch(I /*pos*/,
                      bcd::true_type const& /*is_trivially_destructible*/)
{}

template <typename I>
void destroy_dispatch(I pos,
                      bcd::false_type const& /*is_trivially_destructible*/)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    pos->~value_type();
}

template <typename I>
void destroy(I pos)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    destroy_dispatch(pos, bcd::bool_<bcd::is_trivially_destructible<value_type>::value>());
}

// copy(I, I, O)

template <typename I, typename O>
inline O copy_dispatch(I first, I last, O dst, bcd::true_type const& /*use_memmove*/)
{
   typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
   const std::size_t d = boost::container::iterator_udistance(first, last);
   ::memmove(boost::container::dtl::addressof(*dst), boost::container::dtl::addressof(*first), sizeof(value_type) * d);
   return dst + d;
}

template <typename I, typename O>
inline O copy_dispatch(I first, I last, O dst, bcd::false_type const& /*use_memmove*/)
{
    return std::copy(first, last, dst);                                         // may throw
}

template <typename I, typename O>
inline O copy(I first, I last, O dst)
{
   typedef bcd::bool_
   <  are_corresponding<I, O>::value &&
      bcd::is_trivially_copy_assignable<typename ::boost::container::iterator_traits<O>::value_type>::value
   > use_memmove;

   return copy_dispatch(first, last, dst, use_memmove());                       // may throw
}

// uninitialized_copy(I, I, O)

template <typename I, typename O>
inline
O uninitialized_copy_dispatch(I first, I last, O dst,
                              bcd::true_type const& /*use_memcpy*/)
{
   typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
   const std::size_t d = boost::container::iterator_udistance(first, last);
   ::memcpy(boost::container::dtl::addressof(*dst), boost::container::dtl::addressof(*first), sizeof(value_type) * d);
   return dst + d;
}

template <typename I, typename F>
inline
F uninitialized_copy_dispatch(I first, I last, F dst,
                              bcd::false_type const& /*use_memcpy*/)
{
    return std::uninitialized_copy(first, last, dst);                                       // may throw
}

template <typename I, typename F>
inline
F uninitialized_copy(I first, I last, F dst)
{
   typedef bcd::bool_
   <  are_corresponding<I, F>::value &&
      bcd::is_trivially_copy_constructible<typename ::boost::container::iterator_traits<F>::value_type>::value
   > use_memcpy;
   return uninitialized_copy_dispatch(first, last, dst, use_memcpy());          // may throw
}

// uninitialized_move(I, I, O)

template <typename I, typename O>
inline
O uninitialized_move_dispatch(I first, I last, O dst,
                              bcd::true_type const& /*use_memcpy*/)
{
   typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
   const std::size_t d = boost::container::iterator_udistance(first, last);
   ::memcpy(boost::container::dtl::addressof(*dst), boost::container::dtl::addressof(*first), sizeof(value_type) * d);
   return dst + d;
}

template <typename I, typename O>
inline
O uninitialized_move_dispatch(I first, I last, O dst,
                              bcd::false_type const& /*use_memcpy*/)
{
    //return boost::uninitialized_move(first, last, dst);                         // may throw

    O o = dst;

    BOOST_TRY
    {
        typedef typename boost::container::iterator_traits<O>::value_type value_type;
        for (; first != last; ++first, ++o )
            new (boost::container::dtl::addressof(*o)) value_type(boost::move(*first));
    }
    BOOST_CATCH(...)
    {
        destroy(dst, o);
        BOOST_RETHROW;
    }
    BOOST_CATCH_END

    return dst;
}

template <typename I, typename O>
inline
O uninitialized_move(I first, I last, O dst)
{
   typedef bcd::bool_
   <  are_corresponding<I, O>::value &&
      bcd::is_trivially_copy_constructible<typename ::boost::container::iterator_traits<O>::value_type>::value
   > use_memcpy;
   return uninitialized_move_dispatch(first, last, dst, use_memcpy());         // may throw
}

// TODO - move uses memmove - implement 2nd version using memcpy?

// move(I, I, O)

template <typename I, typename O>
inline
O move_dispatch(I first, I last, O dst,
                bcd::true_type const& /*use_memmove*/)
{
   typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
   const std::size_t d = boost::container::iterator_udistance(first, last);
   ::memmove(boost::container::dtl::addressof(*dst), boost::container::dtl::addressof(*first), sizeof(value_type)*d );
   return dst + d;
}

template <typename I, typename O>
inline
O move_dispatch(I first, I last, O dst,
                bcd::false_type const& /*use_memmove*/)
{
   return boost::move(first, last, dst);                                         // may throw
}

template <typename I, typename O>
inline
O move(I first, I last, O dst)
{
   typedef bcd::bool_
   <  are_corresponding<I, O>::value &&
      bcd::is_trivially_copy_constructible<typename ::boost::container::iterator_traits<O>::value_type>::value
   > use_memmove;
   return move_dispatch(first, last, dst, use_memmove());                      // may throw
}

// move_backward(BDI, BDI, BDO)

template <typename BDI, typename BDO>
inline
BDO move_backward_dispatch(BDI first, BDI last, BDO dst,
                           bcd::true_type const& /*use_memmove*/)
{
    typedef typename ::boost::container::iterator_traits<BDI>::value_type value_type;
    const std::size_t d = boost::container::iterator_udistance(first, last);
    BDO foo(dst - d);
    ::memmove(boost::container::dtl::addressof(*foo), boost::container::dtl::addressof(*first), sizeof(value_type) * d);
    return foo;
}

template <typename BDI, typename BDO>
inline
BDO move_backward_dispatch(BDI first, BDI last, BDO dst,
                           bcd::false_type const& /*use_memmove*/)
{
    return boost::move_backward(first, last, dst);                                // may throw
}

template <typename BDI, typename BDO>
inline
BDO move_backward(BDI first, BDI last, BDO dst)
{
   typedef bcd::bool_
   <  are_corresponding<BDI, BDO>::value &&
      bcd::is_trivially_copy_constructible<typename ::boost::container::iterator_traits<BDO>::value_type>::value
   > use_memmove;
   return move_backward_dispatch(first, last, dst, use_memmove());             // may throw
}

template <typename T>
struct has_nothrow_move : public
    bcd::bool_<
         ::boost::has_nothrow_move<
               typename bcd::remove_const<T>::type
         >::value
        ||
         ::boost::has_nothrow_move<T>::value
    >
{};

// uninitialized_move_if_noexcept(I, I, O)

template <typename I, typename O>
inline
O uninitialized_move_if_noexcept_dispatch(I first, I last, O dst, bcd::true_type const& /*use_move*/)
{ return uninitialized_move(first, last, dst); }

template <typename I, typename O>
inline
O uninitialized_move_if_noexcept_dispatch(I first, I last, O dst, bcd::false_type const& /*use_move*/)
{ return uninitialized_copy(first, last, dst); }

template <typename I, typename O>
inline
O uninitialized_move_if_noexcept(I first, I last, O dst)
{
    typedef has_nothrow_move<
        typename ::boost::container::iterator_traits<O>::value_type
    > use_move;

    return uninitialized_move_if_noexcept_dispatch(first, last, dst, use_move());         // may throw
}

// move_if_noexcept(I, I, O)

template <typename I, typename O>
inline
O move_if_noexcept_dispatch(I first, I last, O dst, bcd::true_type const& /*use_move*/)
{ return move(first, last, dst); }

template <typename I, typename O>
inline
O move_if_noexcept_dispatch(I first, I last, O dst, bcd::false_type const& /*use_move*/)
{ return copy(first, last, dst); }

template <typename I, typename O>
inline
O move_if_noexcept(I first, I last, O dst)
{
    typedef has_nothrow_move<
        typename ::boost::container::iterator_traits<O>::value_type
    > use_move;

    return move_if_noexcept_dispatch(first, last, dst, use_move());         // may throw
}

// uninitialized_fill(I, I)

template <typename I>
inline
void uninitialized_fill_dispatch(I , I ,
                                 bcd::true_type const& /*is_trivially_default_constructible*/,
                                 bcd::true_type const& /*disable_trivial_init*/)
{}

template <typename I>
inline
void uninitialized_fill_dispatch(I first, I last,
                                 bcd::true_type const& /*is_trivially_default_constructible*/,
                                 bcd::false_type const& /*disable_trivial_init*/)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    for ( ; first != last ; ++first )
        new (boost::container::dtl::addressof(*first)) value_type();
}

template <typename I, typename DisableTrivialInit>
inline
void uninitialized_fill_dispatch(I first, I last,
                                 bcd::false_type const& /*is_trivially_default_constructible*/,
                                 DisableTrivialInit const& /*not_used*/)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    I it = first;

    BOOST_TRY
    {
        for ( ; it != last ; ++it )
            new (boost::container::dtl::addressof(*it)) value_type();                           // may throw
    }
    BOOST_CATCH(...)
    {
        destroy(first, it);
        BOOST_RETHROW;
    }
    BOOST_CATCH_END
}

template <typename I, typename DisableTrivialInit>
inline
void uninitialized_fill(I first, I last, DisableTrivialInit const& disable_trivial_init)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    uninitialized_fill_dispatch(first, last
      , bcd::bool_<bcd::is_trivially_default_constructible<value_type>::value>()
      , disable_trivial_init);     // may throw
}

// construct(I)

template <typename I>
inline
void construct_dispatch(bcd::true_type const& /*dont_init*/, I /*pos*/)
{}

template <typename I>
inline
void construct_dispatch(bcd::false_type const& /*dont_init*/, I pos)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    new (static_cast<void*>(::boost::container::dtl::addressof(*pos))) value_type();                      // may throw
}

template <typename DisableTrivialInit, typename I>
inline
void construct(DisableTrivialInit const&, I pos)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type value_type;
    bcd::bool_<
        bcd::is_trivially_default_constructible<value_type>::value &&
        DisableTrivialInit::value
    > dont_init;
    construct_dispatch(dont_init(), pos);                                                // may throw
}

// construct(I, V)

template <typename I, typename V>
inline
void construct_dispatch(I pos, V const& v, bcd::true_type const& /*use_memcpy*/)
{
    ::memcpy(boost::container::dtl::addressof(*pos), boost::container::dtl::addressof(v), sizeof(V));
}

template <typename I, typename P>
inline
void construct_dispatch(I pos, P const& p,
                        bcd::false_type const& /*use_memcpy*/)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type V;
    new (static_cast<void*>(boost::container::dtl::addressof(*pos))) V(p);                      // may throw
}

template <typename DisableTrivialInit, typename I, typename P>
inline
void construct(DisableTrivialInit const&, I pos, P const& p)
{
   typedef bcd::bool_
   <  is_corresponding_value<I, P>::value &&
      bcd::is_trivially_copy_constructible<P>::value
   > use_memcpy;
   construct_dispatch(pos, p, use_memcpy());                                   // may throw
}

// Needed by push_back(V &&)

template <typename DisableTrivialInit, typename I, typename P>
inline
void construct(DisableTrivialInit const&, I pos, BOOST_RV_REF(P) p)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type V;
    new (static_cast<void*>(boost::container::dtl::addressof(*pos))) V(::boost::move(p));                      // may throw
}

// Needed by emplace_back() and emplace()

#if !defined(BOOST_CONTAINER_VARRAY_DISABLE_EMPLACE)
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

template <typename DisableTrivialInit, typename I, class ...Args>
inline
void construct(DisableTrivialInit const&,
               I pos,
               BOOST_FWD_REF(Args) ...args)
{
    typedef typename ::boost::container::iterator_traits<I>::value_type V;
    new (static_cast<void*>(boost::container::dtl::addressof(*pos))) V(::boost::forward<Args>(args)...);    // may throw
}

#else // !BOOST_NO_CXX11_VARIADIC_TEMPLATES

// BOOST_NO_CXX11_RVALUE_REFERENCES -> P0 const& p0
// !BOOST_NO_CXX11_RVALUE_REFERENCES -> P0 && p0
// which means that version with one parameter may take V const& v

#define BOOST_CONTAINER_VARRAY_UTIL_CONSTRUCT_CODE(N) \
template <typename DisableTrivialInit, typename I, typename P BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
inline void construct(DisableTrivialInit const&, I pos, BOOST_FWD_REF(P) p BOOST_MOVE_I##N BOOST_MOVE_UREF##N )\
{\
    typedef typename ::boost::container::iterator_traits<I>::value_type V;\
    new (static_cast<void*>(boost::container::dtl::addressof(*pos)))\
    V(::boost::forward<P>(p) BOOST_MOVE_I##N BOOST_MOVE_FWD##N); /*may throw*/\
}
BOOST_MOVE_ITERATE_1TO9(BOOST_CONTAINER_VARRAY_UTIL_CONSTRUCT_CODE)
#undef BOOST_CONTAINER_VARRAY_UTIL_CONSTRUCT_CODE

#endif // !BOOST_NO_CXX11_VARIADIC_TEMPLATES
#endif // !BOOST_CONTAINER_VARRAY_DISABLE_EMPLACE

// assign(I, V)

template <typename I, typename V>
inline
void assign_dispatch(I pos, V const& v,
                     bcd::true_type const& /*use_memcpy*/)
{
    ::memcpy(boost::container::dtl::addressof(*pos), boost::container::dtl::addressof(v), sizeof(V));
}

template <typename I, typename V>
inline
void assign_dispatch(I pos, V const& v,
                     bcd::false_type const& /*use_memcpy*/)
{
    *pos = v;                                                                   // may throw
}

template <typename I, typename V>
inline
void assign(I pos, V const& v)
{
   typedef bcd::bool_
   <  is_corresponding_value<I, V>::value &&
      bcd::is_trivially_copy_assignable<V>::value
   > use_memcpy;
   assign_dispatch(pos, v, use_memcpy());                                        // may throw
}

template <typename I, typename V>
inline
void assign(I pos, BOOST_RV_REF(V) v)
{
    *pos = boost::move(v);                                                        // may throw
}


// uninitialized_copy_s

template <typename I, typename F>
inline std::size_t uninitialized_copy_s(I first, I last, F dest, std::size_t max_count)
{
    std::size_t count = 0;
    F it = dest;

    BOOST_TRY
    {
        for ( ; first != last ; ++it, ++first, ++count )
        {
            if ( max_count <= count )
                return (std::numeric_limits<std::size_t>::max)();

            // dummy 0 as DisableTrivialInit
            construct(0, it, *first);                                              // may throw
        }
    }
    BOOST_CATCH(...)
    {
        destroy(dest, it);
        BOOST_RETHROW;
    }
    BOOST_CATCH_END

    return count;
}

// scoped_destructor

template<class T>
class scoped_destructor
{
public:
    scoped_destructor(T * ptr) : m_ptr(ptr) {}

    ~scoped_destructor()
    {
        if(m_ptr)
            destroy(m_ptr);
    }

    void release() { m_ptr = 0; }

private:
    T * m_ptr;
};

}}} // namespace boost::container::varray_detail

#endif // BOOST_CONTAINER_DETAIL_VARRAY_UTIL_HPP
