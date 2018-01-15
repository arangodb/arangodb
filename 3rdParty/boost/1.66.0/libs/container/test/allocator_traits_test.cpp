//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/config_begin.hpp>
#include <cstddef>
#include <boost/container/allocator_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/function_detector.hpp>
#include <boost/move/utility_core.hpp>
#include <memory>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
#include <boost/core/lightweight_test.hpp>

template<class T>
class SimpleAllocator
{
   public:
   bool allocate_called_;
   bool deallocate_called_;

   typedef boost::container::container_detail::
      true_type                                 is_always_equal;

   typedef T value_type;

   template <class U>
   SimpleAllocator(SimpleAllocator<U>)
      : allocate_called_(false)
      , deallocate_called_(false)
   {}

   SimpleAllocator()
      : allocate_called_(false)
      , deallocate_called_(false)
   {}

   T* allocate(std::size_t)
   {  allocate_called_ = true; return 0;  }

   void deallocate(T*, std::size_t)
   {  deallocate_called_ = true;  }

   bool allocate_called() const
   {  return allocate_called_;  }

   bool deallocate_called() const
   {  return deallocate_called_;  }

   friend bool operator==(const SimpleAllocator &, const SimpleAllocator &)
   {  return true;  }

   friend bool operator!=(const SimpleAllocator &, const SimpleAllocator &)
   {  return false;  }
};

template<class T>
class SimpleSmartPtr
{
   void unspecified_bool_type_func() const {}
   typedef void (SimpleSmartPtr::*unspecified_bool_type)() const;

   public:

   typedef T* pointer;

   explicit SimpleSmartPtr(pointer p = 0)
    : ptr_(p)
   {}

   SimpleSmartPtr(const SimpleSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   SimpleSmartPtr & operator=(const SimpleSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   operator unspecified_bool_type() const
   {  return ptr_? &SimpleSmartPtr::unspecified_bool_type_func : 0;   }

   private:
   T *ptr_;
};

template<class T>
class ComplexAllocator
{
   public:
   bool allocate_called_;
   bool deallocate_called_;
   bool allocate_hint_called_;
   bool destroy_called_;
   mutable bool max_size_called_;
   mutable bool select_on_container_copy_construction_called_;
   bool construct_called_;
   mutable bool storage_is_unpropagable_;

   typedef T value_type;
   typedef SimpleSmartPtr<T>                    pointer;
   typedef SimpleSmartPtr<const T>              const_pointer;
   typedef typename ::boost::container::
      container_detail::unvoid_ref<T>::type     reference;
   typedef typename ::boost::container::
      container_detail::unvoid_ref<const T>::type     const_reference;
   typedef SimpleSmartPtr<void>                 void_pointer;
   typedef SimpleSmartPtr<const void>           const_void_pointer;
   typedef signed short                         difference_type;
   typedef unsigned short                       size_type;
   typedef boost::container::container_detail::
      true_type                                 propagate_on_container_copy_assignment;
   typedef boost::container::container_detail::
      true_type                                 propagate_on_container_move_assignment;
   typedef boost::container::container_detail::
      true_type                                 propagate_on_container_swap;
   typedef boost::container::container_detail::
      true_type                                 is_partially_propagable;

   ComplexAllocator()
      : allocate_called_(false)
      , deallocate_called_(false)
      , allocate_hint_called_(false)
      , destroy_called_(false)
      , max_size_called_(false)
      , select_on_container_copy_construction_called_(false)
      , construct_called_(false)
   {}

   pointer allocate(size_type)
   {  allocate_called_ = true; return pointer();  }

   void deallocate(pointer, size_type)
   {  deallocate_called_ = true;  }

   //optional
   ComplexAllocator select_on_container_copy_construction() const
   {  select_on_container_copy_construction_called_ = true; return *this;  }

   pointer allocate(size_type n, const const_void_pointer &)
   {  allocate_hint_called_ = true; return allocate(n);  }

   template<class U>
   void destroy(U*)
   {  destroy_called_ = true;  }

   size_type max_size() const
   {  max_size_called_ = true; return size_type(size_type(0)-1);  }

   #if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   #define BOOST_CONTAINER_COMPLEXALLOCATOR_CONSTRUCT_IMPL(N)\
   \
   template< class U BOOST_MOVE_I##N BOOST_MOVE_CLASS##N > \
   void construct(U *p BOOST_MOVE_I##N BOOST_MOVE_UREF##N) \
   {  construct_called_ = true;  ::new(p) U ( BOOST_MOVE_FWD##N ); }\
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_COMPLEXALLOCATOR_CONSTRUCT_IMPL)
   #undef BOOST_CONTAINER_COMPLEXALLOCATOR_CONSTRUCT_IMPL
   #else

   template< class U, class ...Args>
   void construct(U *p, BOOST_FWD_REF(Args) ...args)
   {  construct_called_ = true;  ::new(p) U( ::boost::forward<Args>(args)...); }

   #endif

   template<class U>
   void construct(U *p, boost::container::default_init_t)
   {  construct_called_ = true;  ::new(p)U;   }

   bool storage_is_unpropagable(pointer p) const
   {  storage_is_unpropagable_ = true; return !p;  }

   //getters
   bool allocate_called() const
   {  return allocate_called_;  }

   bool deallocate_called() const
   {  return deallocate_called_;  }

   bool allocate_hint_called() const
   {  return allocate_hint_called_;  }

   bool destroy_called() const
   {  return destroy_called_;  }

   bool max_size_called() const
   {  return max_size_called_;  }

   bool select_on_container_copy_construction_called() const
   {  return select_on_container_copy_construction_called_;  }

   bool construct_called() const
   {  return construct_called_;  }

   bool storage_is_unpropagable_called() const
   {  return storage_is_unpropagable_;  }
};

class copymovable
{
   BOOST_COPYABLE_AND_MOVABLE(copymovable)

   public:

   bool copymoveconstructed_;
   bool moved_;

   copymovable(int, int, int)
      : copymoveconstructed_(false), moved_(false)
   {}

   copymovable()
      : copymoveconstructed_(false), moved_(false)
   {}

   copymovable(const copymovable &)
      : copymoveconstructed_(true), moved_(false)
   {}

   copymovable(BOOST_RV_REF(copymovable))
      : copymoveconstructed_(true), moved_(true)
   {}

   copymovable & operator=(BOOST_COPY_ASSIGN_REF(copymovable) ){ return *this; }
   copymovable & operator=(BOOST_RV_REF(copymovable) ){ return *this; }

   bool copymoveconstructed() const
   {  return copymoveconstructed_;  }

   bool moved() const
   {  return moved_;  }
};

void test_void_allocator()
{
   boost::container::allocator_traits<std::allocator<void>   > stdtraits; (void)stdtraits;
   boost::container::allocator_traits<SimpleAllocator<void>  > simtraits; (void)simtraits;
   boost::container::allocator_traits<ComplexAllocator<void> > comtraits; (void)comtraits;
}

int main()
{
   using namespace boost::container::container_detail;
   test_void_allocator();

   //SimpleAllocator
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::value_type, int>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::pointer, int*>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::const_pointer, const int*>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::void_pointer, void*>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::const_void_pointer, const void*>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::difference_type, std::ptrdiff_t>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::size_type, std::size_t>::value ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < SimpleAllocator<int> >::propagate_on_container_copy_assignment::value == false ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < SimpleAllocator<int> >::propagate_on_container_move_assignment::value == false ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < SimpleAllocator<int> >::propagate_on_container_swap::value == false ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < SimpleAllocator<int> >::is_always_equal::value == true ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < SimpleAllocator<int> >::is_partially_propagable::value == false ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::rebind_traits<double>::allocator_type
                       , SimpleAllocator<double> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < SimpleAllocator<int> >::rebind_alloc<double>::value_type
                       , double >::value ));

   //ComplexAllocator
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::value_type, int>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::pointer,  SimpleSmartPtr<int> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::const_pointer, SimpleSmartPtr<const int> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::void_pointer, SimpleSmartPtr<void> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::const_void_pointer, SimpleSmartPtr<const void> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::difference_type, signed short>::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::size_type, unsigned short>::value ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < ComplexAllocator<int> >::propagate_on_container_copy_assignment::value == true ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < ComplexAllocator<int> >::propagate_on_container_move_assignment::value == true ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < ComplexAllocator<int> >::propagate_on_container_swap::value == true ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < ComplexAllocator<int> >::is_always_equal::value == false ));
   BOOST_STATIC_ASSERT(( boost::container::allocator_traits
                       < ComplexAllocator<int> >::is_partially_propagable::value == true ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::rebind_traits<double>::allocator_type
                       , ComplexAllocator<double> >::value ));
   BOOST_STATIC_ASSERT(( is_same<boost::container::allocator_traits
                       < ComplexAllocator<int> >::rebind_alloc<double>::value_type
                       , double >::value ));

   typedef ComplexAllocator<int> CAlloc;
   typedef SimpleAllocator<int> SAlloc;
   typedef boost::container::allocator_traits<CAlloc> CAllocTraits;
   typedef boost::container::allocator_traits<SAlloc> SAllocTraits;
   CAlloc c_alloc;
   SAlloc s_alloc;

   //allocate
   CAllocTraits::allocate(c_alloc, 1);
   BOOST_TEST(c_alloc.allocate_called());

   SAllocTraits::allocate(s_alloc, 1);
   BOOST_TEST(s_alloc.allocate_called());

   //deallocate
   CAllocTraits::deallocate(c_alloc, CAllocTraits::pointer(), 1);
   BOOST_TEST(c_alloc.deallocate_called());

   SAllocTraits::deallocate(s_alloc, SAllocTraits::pointer(), 1);
   BOOST_TEST(s_alloc.deallocate_called());

   //allocate with hint
   CAllocTraits::allocate(c_alloc, 1, CAllocTraits::const_void_pointer());
   BOOST_TEST(c_alloc.allocate_hint_called());

   s_alloc.allocate_called_ = false;
   SAllocTraits::allocate(s_alloc, 1, SAllocTraits::const_void_pointer());
   BOOST_TEST(s_alloc.allocate_called());

   //destroy
   float dummy;
   CAllocTraits::destroy(c_alloc, &dummy);
   BOOST_TEST(c_alloc.destroy_called());

   SAllocTraits::destroy(s_alloc, &dummy);

   //max_size
   CAllocTraits::max_size(c_alloc);
   BOOST_TEST(c_alloc.max_size_called());

   BOOST_TEST(SAllocTraits::size_type(-1)/sizeof(SAllocTraits::value_type) == SAllocTraits::max_size(s_alloc));

   //select_on_container_copy_construction
   CAllocTraits::select_on_container_copy_construction(c_alloc);
   BOOST_TEST(c_alloc.select_on_container_copy_construction_called());

   SAllocTraits::select_on_container_copy_construction(s_alloc);

   //construct
   {
      copymovable c;
      c.copymoveconstructed_ = true;
      c.copymoveconstructed_ = true;
      CAllocTraits::construct(c_alloc, &c);
      BOOST_TEST(c_alloc.construct_called() && !c.copymoveconstructed() && !c.moved());
   }
   {
      int i = 5;
      CAllocTraits::construct(c_alloc, &i, boost::container::default_init);
      BOOST_TEST(c_alloc.construct_called() && i == 5);
   }
   {
      copymovable c;
      copymovable c2;
      CAllocTraits::construct(c_alloc, &c, c2);
      BOOST_TEST(c_alloc.construct_called() && c.copymoveconstructed() && !c.moved());
   }
   {
      copymovable c;
      copymovable c2;
      CAllocTraits::construct(c_alloc, &c, ::boost::move(c2));
      BOOST_TEST(c_alloc.construct_called() && c.copymoveconstructed() && c.moved());
   }
   {
      copymovable c;
      c.copymoveconstructed_ = true;
      c.copymoveconstructed_ = true;
      SAllocTraits::construct(s_alloc, &c);
      BOOST_TEST(!c.copymoveconstructed() && !c.moved());
   }
   {
      int i = 4;
      SAllocTraits::construct(s_alloc, &i, boost::container::default_init);
      BOOST_TEST(i == 4);
   }
   {
      copymovable c;
      copymovable c2;
      SAllocTraits::construct(s_alloc, &c, c2);
      BOOST_TEST(c.copymoveconstructed() && !c.moved());
   }
   {
      copymovable c;
      copymovable c2;
      SAllocTraits::construct(s_alloc, &c, ::boost::move(c2));
      BOOST_TEST(c.copymoveconstructed() && c.moved());
   }
   {
      copymovable c;
      CAllocTraits::construct(c_alloc, &c, 0, 1, 2);
      BOOST_TEST(c_alloc.construct_called() && !c.copymoveconstructed() && !c.moved());
   }
   {
      copymovable c;
      copymovable c2;
      SAllocTraits::construct(s_alloc, &c, 0, 1, 2);
      BOOST_TEST(!c.copymoveconstructed() && !c.moved());
   }
   //storage_is_unpropagable
   {
      SAlloc s_alloc2;
      BOOST_TEST(!SAllocTraits::storage_is_unpropagable(s_alloc, SAllocTraits::pointer()));
   }
   {
      {
         CAlloc c_alloc2;
         CAlloc::value_type v;
         BOOST_TEST(!CAllocTraits::storage_is_unpropagable(c_alloc, CAllocTraits::pointer(&v)));
         BOOST_TEST(c_alloc.storage_is_unpropagable_called());
      }
      {
         CAlloc c_alloc2;
         BOOST_TEST( CAllocTraits::storage_is_unpropagable(c_alloc2, CAllocTraits::pointer()));
         BOOST_TEST(c_alloc2.storage_is_unpropagable_called());
      }

   }

   return ::boost::report_errors();
}
#include <boost/container/detail/config_end.hpp>
