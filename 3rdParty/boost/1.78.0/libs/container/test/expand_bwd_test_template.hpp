//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_ALLOCATION_TEST_TEMPLATE_HEADER
#define BOOST_CONTAINER_TEST_ALLOCATION_TEST_TEMPLATE_HEADER

#include <boost/container/detail/config_begin.hpp>
#include <vector>
#include <typeinfo>
#include <iostream>
#include "expand_bwd_test_allocator.hpp"
#include <boost/container/detail/algorithm.hpp> //equal()
#include "movable_int.hpp"
#include <boost/move/make_unique.hpp>

namespace boost { namespace container { namespace test {

//Function to check if both sets are equal
template <class Vector1, class Vector2>
bool CheckEqualVector(const Vector1 &vector1, const Vector2 &vector2)
{
   if(vector1.size() != vector2.size())
      return false;
   return boost::container::algo_equal(vector1.begin(), vector1.end(), vector2.begin());
}

template<class Vector>
bool CheckUninitializedIsZero(const Vector & v)
{
   typedef  typename Vector::value_type value_type;
   typename Vector::size_type sz    = v.size();
   typename Vector::size_type extra = v.capacity() - v.size();
   value_type comp(0);

   const value_type *holder = &v[0] + sz;

   while(extra--){
      if(*holder++ != comp)
         return false;
   }
   return true;
}


//This function tests all the possible combinations when
//inserting data in a vector and expanding backwards
template<class VectorWithExpandBwdAllocator>
bool test_insert_with_expand_bwd()
{
   typedef typename VectorWithExpandBwdAllocator::value_type value_type;
   typedef std::vector<value_type> Vect;
   const unsigned int MemorySize = 1000;

   //Distance old and new buffer
   const unsigned int Offset[]      =
      {  350, 300, 250, 200, 150, 100, 150, 100,
         150,  50,  50,  50    };
   //Initial vector size
   const unsigned int InitialSize[] =
      {  200, 200, 200, 200, 200, 200, 200, 200,
         200, 200, 200, 200   };
   //Size of the data to insert
   const unsigned int InsertSize[]  =
      {  100, 100, 100, 100, 100, 100, 200, 200,
         300,  25, 100, 200   };
   //Number of tests
   const unsigned int Iterations    = sizeof(InsertSize)/sizeof(int);

   //Insert position
   const int Position[]    =
      {  0, 100,  200  };

   for(unsigned int pos = 0; pos < sizeof(Position)/sizeof(Position[0]); ++pos){
      if(!life_count<value_type>::check(0))
         return false;

      for(unsigned int iteration = 0; iteration < Iterations; ++iteration)
      {
         boost::movelib::unique_ptr<char[]> memptr =
            boost::movelib::make_unique_definit<char[]>(MemorySize*sizeof(value_type));
         value_type *memory = (value_type*)memptr.get();
         std::vector<value_type> initial_data;
         initial_data.resize(InitialSize[iteration]);
         for(unsigned int i = 0; i < InitialSize[iteration]; ++i){
            initial_data[i] = static_cast<value_type>((int)i);
         }

         if(!life_count<value_type>::check(InitialSize[iteration]))
            return false;
         Vect data_to_insert;
         data_to_insert.resize(InsertSize[iteration]);
         for(unsigned int i = 0; i < InsertSize[iteration]; ++i){
            data_to_insert[i] = static_cast<value_type>((int)-i);
         }

         if(!life_count<value_type>::check(InitialSize[iteration]+InsertSize[iteration]))
            return false;

         expand_bwd_test_allocator<value_type> alloc
            (memory, MemorySize, Offset[iteration]);
         VectorWithExpandBwdAllocator vector(alloc);
         vector.insert( vector.begin()
                     , initial_data.begin(), initial_data.end());
         vector.insert( vector.begin() + Position[pos]
                     , data_to_insert.begin(), data_to_insert.end());

         if(!life_count<value_type>::check(InitialSize[iteration]*2+InsertSize[iteration]*2))
            return false;

         initial_data.insert(initial_data.begin() + Position[pos]
                           , data_to_insert.begin(), data_to_insert.end());
         //Now check that values are equal
         if(!CheckEqualVector(vector, initial_data)){
            std::cout << "test_assign_with_expand_bwd::CheckEqualVector failed." << std::endl
                     << "   Class: " << typeid(VectorWithExpandBwdAllocator).name() << std::endl
                     << "   Iteration: " << iteration << std::endl;
            return false;
         }
      }
      if(!life_count<value_type>::check(0))
         return false;
   }

   return true;
}

//This function tests all the possible combinations when
//inserting data in a vector and expanding backwards
template<class VectorWithExpandBwdAllocator>
bool test_assign_with_expand_bwd()
{
   typedef typename VectorWithExpandBwdAllocator::value_type value_type;
   const unsigned int MemorySize = 200;

   const unsigned int Offset[]      = { 50, 50, 50};
   const unsigned int InitialSize[] = { 25, 25, 25};
   const unsigned int InsertSize[]  = { 15, 35, 55};
   const unsigned int Iterations    = sizeof(InsertSize)/sizeof(int);

   for(unsigned int iteration = 0; iteration <Iterations; ++iteration)
   {
      boost::movelib::unique_ptr<char[]> memptr =
         boost::movelib::make_unique_definit<char[]>(MemorySize*sizeof(value_type));
      value_type *memory = (value_type*)memptr.get();
      //Create initial data
      std::vector<value_type> initial_data;
      initial_data.resize(InitialSize[iteration]);
      for(unsigned int i = 0; i < InitialSize[iteration]; ++i){
         initial_data[i] = static_cast<value_type>((int)i);
      }

      //Create data to assign
      std::vector<value_type> data_to_insert;
      data_to_insert.resize(InsertSize[iteration]);
      for(unsigned int i = 0; i < InsertSize[iteration]; ++i){
         data_to_insert[i] = static_cast<value_type>((int)-i);
      }

      //Insert initial data to the vector to test
      expand_bwd_test_allocator<value_type> alloc
         (memory, MemorySize, Offset[iteration]);
      VectorWithExpandBwdAllocator vector(alloc);
      vector.insert( vector.begin()
                  , initial_data.begin(), initial_data.end());

      //Assign data
      vector.insert(vector.cbegin(), data_to_insert.begin(), data_to_insert.end());
      initial_data.insert(initial_data.begin(), data_to_insert.begin(), data_to_insert.end());

      //Now check that values are equal
      if(!CheckEqualVector(vector, initial_data)){
         std::cout << "test_assign_with_expand_bwd::CheckEqualVector failed." << std::endl
                  << "   Class: " << typeid(VectorWithExpandBwdAllocator).name() << std::endl
                  << "   Iteration: " << iteration << std::endl;
         return false;
      }
   }

   return true;
}

//This function calls all tests
template<class VectorWithExpandBwdAllocator>
bool test_all_expand_bwd()
{
   std::cout << "Starting test_insert_with_expand_bwd." << std::endl << "  Class: "
             << typeid(VectorWithExpandBwdAllocator).name() << std::endl;

   if(!test_insert_with_expand_bwd<VectorWithExpandBwdAllocator>()){
      std::cout << "test_allocation_direct_deallocation failed. Class: "
                << typeid(VectorWithExpandBwdAllocator).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_assign_with_expand_bwd." << std::endl << "  Class: "
             << typeid(VectorWithExpandBwdAllocator).name() << std::endl;

   if(!test_assign_with_expand_bwd<VectorWithExpandBwdAllocator>()){
      std::cout << "test_allocation_direct_deallocation failed. Class: "
                << typeid(VectorWithExpandBwdAllocator).name() << std::endl;
      return false;
   }

   return true;
}

}}}   //namespace boost { namespace container { namespace test {

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_TEST_ALLOCATION_TEST_TEMPLATE_HEADER
