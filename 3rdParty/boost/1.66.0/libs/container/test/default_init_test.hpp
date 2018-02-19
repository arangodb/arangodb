//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_DEFAULT_INIT_TEST_HEADER
#define BOOST_CONTAINER_TEST_DEFAULT_INIT_TEST_HEADER

#include <boost/container/detail/config_begin.hpp>
#include <cstddef>

namespace boost{
namespace container {
namespace test{

//
template<int Dummy = 0>
class default_init_allocator_base
{
   protected:
   static unsigned char s_pattern;
   static bool          s_ascending;

   public:
   static void reset_pattern(unsigned char value)
   {  s_pattern = value;   }

   static void set_ascending(bool enable)
   {  s_ascending = enable;   }
};

template<int Dummy>
unsigned char default_init_allocator_base<Dummy>::s_pattern = 0u;

template<int Dummy>
bool default_init_allocator_base<Dummy>::s_ascending = true;

template<class Integral>
class default_init_allocator
   : public default_init_allocator_base<0>
{
   typedef default_init_allocator_base<0> base_t;
   public:
   typedef Integral value_type;

   default_init_allocator()
   {}

   template <class U>
   default_init_allocator(default_init_allocator<U>)
   {}

   Integral* allocate(std::size_t n)
   {
      //Initialize memory to a pattern
      const std::size_t max = sizeof(Integral)*n;
      unsigned char *puc_raw = ::new unsigned char[max];

      if(base_t::s_ascending){
         for(std::size_t i = 0; i != max; ++i){
            puc_raw[i] = static_cast<unsigned char>(s_pattern++);
         }
      }
      else{
         for(std::size_t i = 0; i != max; ++i){
            puc_raw[i] = static_cast<unsigned char>(s_pattern--);
         }
      }
      return (Integral*)puc_raw;;
   }

   void deallocate(Integral *p, std::size_t)
   {  delete[] (unsigned char*)p;  }
};

template<class Integral>
inline bool check_ascending_byte_pattern(const Integral&t)
{
   const unsigned char *pch = &reinterpret_cast<const unsigned char &>(t);
   const std::size_t max = sizeof(Integral);
   for(std::size_t i = 1; i != max; ++i){
      if( (pch[i-1] != ((unsigned char)(pch[i]-1u))) ){
         return false;
      }
   }
   return true;
}

template<class Integral>
inline bool check_descending_byte_pattern(const Integral&t)
{
   const unsigned char *pch = &reinterpret_cast<const unsigned char &>(t);
   const std::size_t max = sizeof(Integral);
   for(std::size_t i = 1; i != max; ++i){
      if( (pch[i-1] != ((unsigned char)(pch[i]+1u))) ){
         return false;
      }
   }
   return true;
}

template<class IntDefaultInitAllocVector>
bool default_init_test()//Test for default initialization
{
   const std::size_t Capacity = 100;

   {
      test::default_init_allocator<int>::reset_pattern(0);
      test::default_init_allocator<int>::set_ascending(true);
      IntDefaultInitAllocVector v(Capacity, default_init);
      typename IntDefaultInitAllocVector::iterator it = v.begin();
      //Compare with the pattern
      for(std::size_t i = 0; i != Capacity; ++i, ++it){
         if(!test::check_ascending_byte_pattern(*it))
            return false;
      }
   }
   {
      test::default_init_allocator<int>::reset_pattern(0);
      test::default_init_allocator<int>::set_ascending(true);
      IntDefaultInitAllocVector v(Capacity, default_init, typename IntDefaultInitAllocVector::allocator_type());
      typename IntDefaultInitAllocVector::iterator it = v.begin();
      //Compare with the pattern
      for(std::size_t i = 0; i != Capacity; ++i, ++it){
         if(!test::check_ascending_byte_pattern(*it))
            return false;
      }
   }
   {
      test::default_init_allocator<int>::reset_pattern(100);
      test::default_init_allocator<int>::set_ascending(false);
      IntDefaultInitAllocVector v;
      v.resize(Capacity, default_init);
      typename IntDefaultInitAllocVector::iterator it = v.begin();
      //Compare with the pattern
      for(std::size_t i = 0; i != Capacity; ++i, ++it){
         if(!test::check_descending_byte_pattern(*it))
            return false;
      }
   }
   return true;
}

}  //namespace test{
}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif //BOOST_CONTAINER_TEST_DEFAULT_INIT_TEST_HEADER
