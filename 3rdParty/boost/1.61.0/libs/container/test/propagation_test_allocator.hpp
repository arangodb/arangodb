//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_ALLOCATOR_ARGUMENT_TESTER_HPP
#define BOOST_CONTAINER_TEST_ALLOCATOR_ARGUMENT_TESTER_HPP

#include <boost/container/uses_allocator.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/move/core.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>


template<class T, unsigned int Id, bool HasTrueTypes = false>
class propagation_test_allocator
{
   BOOST_COPYABLE_AND_MOVABLE(propagation_test_allocator)
   public:

   template<class U>
   struct rebind
   {
      typedef propagation_test_allocator<U, Id, HasTrueTypes> other;
   };

   typedef boost::container::container_detail::bool_<HasTrueTypes>  propagate_on_container_copy_assignment;
   typedef boost::container::container_detail::bool_<HasTrueTypes>  propagate_on_container_move_assignment;
   typedef boost::container::container_detail::bool_<HasTrueTypes>  propagate_on_container_swap;
   typedef boost::container::container_detail::bool_<HasTrueTypes>  is_always_equal;
   typedef T value_type;

   propagation_test_allocator()
      : m_default_contructed(true), m_move_contructed(false), m_move_assigned(false)
   {}

   propagation_test_allocator(const propagation_test_allocator&)
      : m_default_contructed(false), m_move_contructed(false), m_move_assigned(false)
   {}

   propagation_test_allocator(BOOST_RV_REF(propagation_test_allocator) )
      : m_default_contructed(false), m_move_contructed(true), m_move_assigned(false)
   {}

   template<class U>
   propagation_test_allocator(BOOST_RV_REF_BEG propagation_test_allocator<U, Id, HasTrueTypes> BOOST_RV_REF_END)
      : m_default_contructed(false), m_move_contructed(true), m_move_assigned(false)
   {}

   template<class U>
   propagation_test_allocator(const propagation_test_allocator<U, Id, HasTrueTypes> &)
   {}

   propagation_test_allocator & operator=(BOOST_COPY_ASSIGN_REF(propagation_test_allocator))
   {  return *this;  }

   propagation_test_allocator & operator=(BOOST_RV_REF(propagation_test_allocator))
   {
      m_move_assigned = true;
      return *this;
   }

   std::size_t max_size() const
   {  return std::size_t(-1);  }

   T* allocate(std::size_t n)
   {  return (T*)::new char[n*sizeof(T)];  }

   void deallocate(T*p, std::size_t)
   {  delete []static_cast<char*>(static_cast<void*>(p));  }

   bool m_default_contructed;
   bool m_move_contructed;
   bool m_move_assigned;
};

template <class T1, class T2, unsigned int Id, bool HasTrueTypes>
bool operator==( const propagation_test_allocator<T1, Id, HasTrueTypes>&
               , const propagation_test_allocator<T2, Id, HasTrueTypes>&)
{  return true;   }

template <class T1, class T2, unsigned int Id, bool HasTrueTypes>
bool operator!=( const propagation_test_allocator<T1, Id, HasTrueTypes>&
               , const propagation_test_allocator<T2, Id, HasTrueTypes>&)
{  return false;   }

//This enum lists the construction options
//for an allocator-aware type
enum ConstructionTypeEnum
{
   ConstructiblePrefix,
   ConstructibleSuffix,
   ErasedTypePrefix,
   ErasedTypeSuffix,
   NotUsesAllocator
};

//This base class provices types for
//the derived class to implement each construction
//type. If a construction type does not apply
//the typedef is set to an internal nat
//so that the class is not constructible from
//the user arguments.
template<ConstructionTypeEnum ConstructionType, unsigned int AllocatorTag>
struct uses_allocator_base;

template<unsigned int AllocatorTag>
struct uses_allocator_base<ConstructibleSuffix, AllocatorTag>
{
   typedef propagation_test_allocator<int, AllocatorTag> allocator_type;
   typedef allocator_type allocator_constructor_type;
   struct nat{};
   typedef nat allocator_arg_type;
};

template<unsigned int AllocatorTag>
struct uses_allocator_base<ConstructiblePrefix, AllocatorTag>
{
   typedef propagation_test_allocator<int, AllocatorTag> allocator_type;
   typedef allocator_type allocator_constructor_type;
   typedef boost::container::allocator_arg_t allocator_arg_type;
};

template<unsigned int AllocatorTag>
struct uses_allocator_base<ErasedTypePrefix, AllocatorTag>
{
   typedef boost::container::erased_type allocator_type;
   typedef boost::container::pmr::polymorphic_allocator<int> allocator_constructor_type;
   typedef boost::container::allocator_arg_t allocator_arg_type;
};

template<unsigned int AllocatorTag>
struct uses_allocator_base<ErasedTypeSuffix, AllocatorTag>
{
   typedef boost::container::erased_type allocator_type;
   typedef boost::container::pmr::polymorphic_allocator<int> allocator_constructor_type;
   struct nat{};
   typedef nat allocator_arg_type;
};

template<unsigned int AllocatorTag>
struct uses_allocator_base<NotUsesAllocator, AllocatorTag>
{
   struct nat{};
   typedef nat allocator_constructor_type;
   typedef nat allocator_arg_type;
};

template<ConstructionTypeEnum ConstructionType, unsigned int AllocatorTag>
struct allocator_argument_tester
   : uses_allocator_base<ConstructionType, AllocatorTag>
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(allocator_argument_tester)

   public:

   typedef uses_allocator_base<ConstructionType, AllocatorTag> base_type;

   //0 user argument constructors
   allocator_argument_tester()
      : construction_type(NotUsesAllocator), value(0)
   {}

   explicit allocator_argument_tester
      (typename base_type::allocator_constructor_type)
      : construction_type(ConstructibleSuffix), value(0)
   {}

   explicit allocator_argument_tester
      (typename base_type::allocator_arg_type, typename base_type::allocator_constructor_type)
      : construction_type(ConstructiblePrefix), value(0)
   {}

   //1 user argument constructors
   explicit allocator_argument_tester(int i)
      : construction_type(NotUsesAllocator), value(i)
   {}

   allocator_argument_tester
      (int i, typename base_type::allocator_constructor_type)
      : construction_type(ConstructibleSuffix), value(i)
   {}

   allocator_argument_tester
      ( typename base_type::allocator_arg_type
      , typename base_type::allocator_constructor_type
      , int i)
      : construction_type(ConstructiblePrefix), value(i)
   {}

   //Copy constructors
   allocator_argument_tester(const allocator_argument_tester &other)
      : construction_type(NotUsesAllocator), value(other.value)
   {}

   allocator_argument_tester( const allocator_argument_tester &other
                            , typename base_type::allocator_constructor_type)
      : construction_type(ConstructibleSuffix), value(other.value)
   {}

   allocator_argument_tester( typename base_type::allocator_arg_type
                            , typename base_type::allocator_constructor_type
                            , const allocator_argument_tester &other)
      : construction_type(ConstructiblePrefix), value(other.value)
   {}

   //Move constructors
   allocator_argument_tester(BOOST_RV_REF(allocator_argument_tester) other)
      : construction_type(NotUsesAllocator), value(other.value)
   {  other.value = 0;  other.construction_type = NotUsesAllocator;  }

   allocator_argument_tester( BOOST_RV_REF(allocator_argument_tester) other
                            , typename base_type::allocator_constructor_type)
      : construction_type(ConstructibleSuffix), value(other.value)
   {  other.value = 0;  other.construction_type = ConstructibleSuffix;  }

   allocator_argument_tester( typename base_type::allocator_arg_type
                            , typename base_type::allocator_constructor_type
                            , BOOST_RV_REF(allocator_argument_tester) other)
      : construction_type(ConstructiblePrefix), value(other.value)
   {  other.value = 0;  other.construction_type = ConstructiblePrefix;  }

   ConstructionTypeEnum construction_type;
   int                  value;
};

namespace boost {
namespace container {

template<unsigned int AllocatorTag>
struct constructible_with_allocator_prefix
   < ::allocator_argument_tester<ConstructiblePrefix, AllocatorTag> >
{
   static const bool value = true;
};

template<unsigned int AllocatorTag>
struct constructible_with_allocator_prefix
   < ::allocator_argument_tester<ErasedTypePrefix, AllocatorTag> >
{
   static const bool value = true;
};


template<unsigned int AllocatorTag>
struct constructible_with_allocator_suffix
   < ::allocator_argument_tester<ConstructibleSuffix, AllocatorTag> >
{
   static const bool value = true;
};

template<unsigned int AllocatorTag>
struct constructible_with_allocator_suffix
   < ::allocator_argument_tester<ErasedTypeSuffix, AllocatorTag> >
{
   static const bool value = true;
};

}  //namespace container {
}  //namespace boost {

#endif   //BOOST_CONTAINER_TEST_ALLOCATOR_ARGUMENT_TESTER_HPP
