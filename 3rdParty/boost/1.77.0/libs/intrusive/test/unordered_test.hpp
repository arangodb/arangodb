/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2015-2015.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/detail/iterator.hpp>
#include "common_functors.hpp"
#include <vector>
#include <algorithm> //std::sort
#include <set>
#include <boost/core/lightweight_test.hpp>

#include "test_macros.hpp"
#include "test_container.hpp"
#include "unordered_test_common.hpp"

namespace boost{
namespace intrusive{
namespace test{

static const std::size_t BucketSize = 8;

template<class ContainerDefiner>
struct test_unordered
{
   typedef typename ContainerDefiner::value_cont_type value_cont_type;

   static void test_all(value_cont_type& values);
   private:
   static void test_sort(value_cont_type& values);
   static void test_insert(value_cont_type& values, detail::true_);
   static void test_insert(value_cont_type& values, detail::false_);
   static void test_swap(value_cont_type& values);
   static void test_rehash(value_cont_type& values, detail::true_);
   static void test_rehash(value_cont_type& values, detail::false_);
   static void test_find(value_cont_type& values);
   static void test_impl();
   static void test_clone(value_cont_type& values);
};

template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_all (value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;
   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;
   {
      typename unordered_type::bucket_type buckets [BucketSize];
      unordered_type testset
         (bucket_traits(pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));
      testset.insert(values.begin(), values.end());
      test::test_container(testset);
      testset.clear();
      testset.insert(values.begin(), values.end());
      test::test_common_unordered_and_associative_container(testset, values);
      testset.clear();
      testset.insert(values.begin(), values.end());
      test::test_unordered_associative_container(testset, values);
      testset.clear();
      testset.insert(values.begin(), values.end());
      typedef detail::bool_<boost::intrusive::test::is_multikey_true
         <unordered_type>::value> select_t;
      test::test_maybe_unique_container(testset, values, select_t());
   }
   {
      value_cont_type vals(BucketSize);
      for (int i = 0; i < (int)BucketSize; ++i)
         (&vals[i])->value_ = i;
      typename unordered_type::bucket_type buckets [BucketSize];
      unordered_type testset(bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));
      testset.insert(vals.begin(), vals.end());
      test::test_iterator_forward(testset);
   }
   test_sort(values);
   test_insert(values, detail::bool_<boost::intrusive::test::is_multikey_true<unordered_type>::value>());
   test_swap(values);
   test_rehash(values, detail::bool_<unordered_type::incremental>());
   test_find(values);
   test_impl();
   test_clone(values);
}

//test case due to an error in tree implementation:
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_impl()
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;
   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;

   value_cont_type values (5);
   for (int i = 0; i < 5; ++i)
      values[i].value_ = i;

   typename unordered_type::bucket_type buckets [BucketSize];
   unordered_type testset(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));

   for (int i = 0; i < 5; ++i)
      testset.insert (values[i]);

   testset.erase (testset.iterator_to (values[0]));
   testset.erase (testset.iterator_to (values[1]));
   testset.insert (values[1]);

   testset.erase (testset.iterator_to (values[2]));
   testset.erase (testset.iterator_to (values[3]));
}

//test: constructor, iterator, clear, reverse_iterator, front, back, size:
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_sort(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;
   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;

   typename unordered_type::bucket_type buckets [BucketSize];
   unordered_type testset1
      (values.begin(), values.end(), bucket_traits
         (pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));

   if(unordered_type::incremental){
      {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   }
   else{
      {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   }
   testset1.clear();
   BOOST_TEST (testset1.empty());
}

//test: insert, const_iterator, const_reverse_iterator, erase, iterator_to:
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_insert(value_cont_type& values, detail::false_) //not multikey
{

   typedef typename ContainerDefiner::template container
      <>::type unordered_set_type;
   typedef typename unordered_set_type::bucket_traits bucket_traits;
   typedef typename unordered_set_type::key_of_value  key_of_value;

   typename unordered_set_type::bucket_type buckets [BucketSize];
   unordered_set_type testset(bucket_traits(
      pointer_traits<typename unordered_set_type::bucket_ptr>::
         pointer_to(buckets[0]), BucketSize));
   testset.insert(&values[0] + 2, &values[0] + 5);

   typename unordered_set_type::insert_commit_data commit_data;
   BOOST_TEST ((!testset.insert_check(key_of_value()(values[2]), commit_data).second));
   BOOST_TEST (( testset.insert_check(key_of_value()(values[0]), commit_data).second));

   const unordered_set_type& const_testset = testset;
   if(unordered_set_type::incremental)
   {
      {  int init_values [] = { 4, 5, 1 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
      typename unordered_set_type::iterator i = testset.begin();
      BOOST_TEST (i->value_ == 4);

      i = testset.insert(values[0]).first;
      BOOST_TEST (&*i == &values[0]);

      i = testset.iterator_to (values[2]);
      BOOST_TEST (&*i == &values[2]);

      testset.erase (i);

      {  int init_values [] = { 5, 1, 3 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
   }
   else{
      {  int init_values [] = { 1, 4, 5 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
      typename unordered_set_type::iterator i = testset.begin();
      BOOST_TEST (i->value_ == 1);

      i = testset.insert(values[0]).first;
      BOOST_TEST (&*i == &values[0]);

      i = testset.iterator_to (values[2]);
      BOOST_TEST (&*i == &values[2]);

      testset.erase (i);

      {  int init_values [] = { 1, 3, 5 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
   }
}

template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_insert(value_cont_type& values, detail::true_) //is multikey
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;

   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr bucket_ptr;
   typedef typename unordered_type::iterator iterator;
   typedef typename unordered_type::key_type key_type;
   {
      typename unordered_type::bucket_type buckets [BucketSize];
      unordered_type testset(bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));

      testset.insert(&values[0] + 2, &values[0] + 5);

      const unordered_type& const_testset = testset;

      if(unordered_type::incremental){
         {
            {  int init_values [] = { 4, 5, 1 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }

            typename unordered_type::iterator i = testset.begin();
            BOOST_TEST (i->value_ == 4);

            i = testset.insert (values[0]);
            BOOST_TEST (&*i == &values[0]);

            i = testset.iterator_to (values[2]);
            BOOST_TEST (&*i == &values[2]);
            testset.erase(i);

            {  int init_values [] = { 5, 1, 3 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
            testset.clear();
            testset.insert(&values[0], &values[0] + values.size());

            {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }

            BOOST_TEST (testset.erase(key_type(1)) == 1);
            BOOST_TEST (testset.erase(key_type(2)) == 2);
            BOOST_TEST (testset.erase(key_type(3)) == 1);
            BOOST_TEST (testset.erase(key_type(4)) == 1);
            BOOST_TEST (testset.erase(key_type(5)) == 1);
            BOOST_TEST (testset.empty() == true);

            //Now with a single bucket
            typename unordered_type::bucket_type single_bucket[1];
            unordered_type testset2(bucket_traits(
               pointer_traits<bucket_ptr>::pointer_to(single_bucket[0]), 1));
            testset2.insert(&values[0], &values[0] + values.size());
            BOOST_TEST (testset2.erase(key_type(5)) == 1);
            BOOST_TEST (testset2.erase(key_type(2)) == 2);
            BOOST_TEST (testset2.erase(key_type(1)) == 1);
            BOOST_TEST (testset2.erase(key_type(4)) == 1);
            BOOST_TEST (testset2.erase(key_type(3)) == 1);
            BOOST_TEST (testset2.empty() == true);
         }
      }
      else{
         {
            {  int init_values [] = { 1, 4, 5 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }

            typename unordered_type::iterator i = testset.begin();
            BOOST_TEST (i->value_ == 1);

            i = testset.insert (values[0]);
            BOOST_TEST (&*i == &values[0]);

            i = testset.iterator_to (values[2]);
            BOOST_TEST (&*i == &values[2]);
            testset.erase(i);

            {  int init_values [] = { 1, 3, 5 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }
            testset.clear();
            testset.insert(&values[0], &values[0] + values.size());

            {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
               TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, const_testset );  }

            BOOST_TEST (testset.erase(key_type(1)) == 1);
            BOOST_TEST (testset.erase(key_type(2)) == 2);
            BOOST_TEST (testset.erase(key_type(3)) == 1);
            BOOST_TEST (testset.erase(key_type(4)) == 1);
            BOOST_TEST (testset.erase(key_type(5)) == 1);
            BOOST_TEST (testset.empty() == true);

            //Now with a single bucket
            typename unordered_type::bucket_type single_bucket[1];
            unordered_type testset2(bucket_traits(
               pointer_traits<bucket_ptr>::pointer_to(single_bucket[0]), 1));
            testset2.insert(&values[0], &values[0] + values.size());
            BOOST_TEST (testset2.erase(key_type(5)) == 1);
            BOOST_TEST (testset2.erase(key_type(2)) == 2);
            BOOST_TEST (testset2.erase(key_type(1)) == 1);
            BOOST_TEST (testset2.erase(key_type(4)) == 1);
            BOOST_TEST (testset2.erase(key_type(3)) == 1);
            BOOST_TEST (testset2.empty() == true);
         }
      }
      {
         //Now erase just one per loop
         const int random_init[] = { 3, 2, 4, 1, 5, 2, 2 };
         const unsigned int random_size = sizeof(random_init)/sizeof(random_init[0]);
         typename unordered_type::bucket_type single_bucket[1];
         for(unsigned int i = 0, max = random_size; i != max; ++i){
            value_cont_type data (random_size);
            for (unsigned int j = 0; j < random_size; ++j)
               data[j].value_ = random_init[j];
            unordered_type testset_new(bucket_traits(
               pointer_traits<bucket_ptr>::pointer_to(single_bucket[0]), 1));
            testset_new.insert(&data[0], &data[0]+max);
            testset_new.erase(testset_new.iterator_to(data[i]));
            BOOST_TEST (testset_new.size() == (max -1));
         }
      }
   }
   {
      const unsigned int LoadFactor    = 3;
      const unsigned int NumIterations = BucketSize*LoadFactor;
      value_cont_type random_init(NumIterations);//Preserve memory
      value_cont_type set_tester;
      set_tester.reserve(NumIterations);

      //Initialize values
      for (unsigned int i = 0; i < NumIterations; ++i){
         random_init[i].value_ = i*2;//(i/LoadFactor)*LoadFactor;
      }

      typename unordered_type::bucket_type buckets [BucketSize];
      bucket_traits btraits(pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize);

      for(unsigned int initial_pos = 0; initial_pos != (NumIterations+1); ++initial_pos){
         for(unsigned int final_pos = initial_pos; final_pos != (NumIterations+1); ++final_pos){

            //Create intrusive container inserting values
            unordered_type testset
               ( random_init.data()
               , random_init.data() + random_init.size()
               , btraits);

            BOOST_TEST (testset.size() == random_init.size());

            //Obtain the iterator range to erase
            iterator it_beg_pos = testset.begin();
            for(unsigned int it_beg_pos_num = 0; it_beg_pos_num != initial_pos; ++it_beg_pos_num){
               ++it_beg_pos;
            }
            iterator it_end_pos(it_beg_pos);
            for(unsigned int it_end_pos_num = 0; it_end_pos_num != (final_pos - initial_pos); ++it_end_pos_num){
               ++it_end_pos;
            }

            //Erase the same values in both the intrusive and original vector
            std::size_t erased_cnt = boost::intrusive::iterator_distance(it_beg_pos, it_end_pos);

            //Erase values from the intrusive container
            testset.erase(it_beg_pos, it_end_pos);

            BOOST_TEST (testset.size() == (random_init.size()-(final_pos - initial_pos)));

            //Now test...
            BOOST_TEST ((random_init.size() - erased_cnt) == testset.size());

            //Create an ordered copy of the intrusive container
            set_tester.insert(set_tester.end(), testset.begin(), testset.end());
            std::sort(set_tester.begin(), set_tester.end());
            {
               typename value_cont_type::iterator it = set_tester.begin(), itend = set_tester.end();
               typename value_cont_type::iterator random_init_it(random_init.begin());
               for( ; it != itend; ++it){
                  while(!random_init_it->is_linked())
                     ++random_init_it;
                  BOOST_TEST(*it == *random_init_it);
                  ++random_init_it;
               }
            }
            set_tester.clear();
         }
      }
   }
}

//test: insert (seq-version), swap, erase (seq-version), size:
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_swap(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;

   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;
   typename unordered_type::bucket_type buckets [BucketSize];

   typename unordered_type::bucket_type buckets2 [BucketSize];
   unordered_type testset1(&values[0], &values[0] + 2,
      bucket_traits(pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));
   unordered_type testset2(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets2[0]), BucketSize));

   testset2.insert (&values[0] + 2, &values[0] + 6);
   testset1.swap (testset2);

   if(unordered_type::incremental){
      {  int init_values [] = { 4, 5, 1, 2 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

      {  int init_values [] = { 2, 3 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset2 );  }
      testset1.erase (testset1.iterator_to(values[4]), testset1.end());
      BOOST_TEST (testset1.size() == 1);
      //  BOOST_TEST (&testset1.front() == &values[3]);
      BOOST_TEST (&*testset1.begin() == &values[2]);
   }
   else{
      {  int init_values [] = { 1, 2, 4, 5 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

      {  int init_values [] = { 2, 3 };
         TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset2 );  }
      testset1.erase (testset1.iterator_to(values[5]), testset1.end());
      BOOST_TEST (testset1.size() == 1);
      //  BOOST_TEST (&testset1.front() == &values[3]);
      BOOST_TEST (&*testset1.begin() == &values[3]);
   }
}



//test: rehash:

template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_rehash(value_cont_type& values, detail::true_)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;

   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr bucket_ptr;
   //Build a uset
   typename unordered_type::bucket_type buckets1 [BucketSize];
   typename unordered_type::bucket_type buckets2 [BucketSize*2];
   unordered_type testset1(&values[0], &values[0] + values.size(),
      bucket_traits(pointer_traits<bucket_ptr>::
         pointer_to(buckets1[0]), BucketSize));
   //Test current state
   BOOST_TEST(testset1.split_count() == BucketSize/2);
   {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   //Incremental rehash step
   BOOST_TEST (testset1.incremental_rehash() == true);
   BOOST_TEST(testset1.split_count() == (BucketSize/2+1));
   {  int init_values [] = { 5, 1, 2, 2, 3, 4 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   //Rest of incremental rehashes should lead to the same sequence
   for(std::size_t split_bucket = testset1.split_count(); split_bucket != BucketSize; ++split_bucket){
      BOOST_TEST (testset1.incremental_rehash() == true);
      BOOST_TEST(testset1.split_count() == (split_bucket+1));
      {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   }
   //This incremental rehash should fail because we've reached the end of the bucket array
   BOOST_TEST (testset1.incremental_rehash() == false);
   BOOST_TEST(testset1.split_count() == BucketSize);
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //
   //Try incremental hashing specifying a new bucket traits pointing to the same array
   //
   //This incremental rehash should fail because the new size is not twice the original
   BOOST_TEST(testset1.incremental_rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
                              pointer_to(buckets1[0]), BucketSize)) == false);
   BOOST_TEST(testset1.split_count() == BucketSize);
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //
   //Try incremental hashing specifying a new bucket traits pointing to the same array
   //
   //This incremental rehash should fail because the new size is not twice the original
   BOOST_TEST(testset1.incremental_rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
               pointer_to(buckets2[0]), BucketSize)) == false);
   BOOST_TEST(testset1.split_count() == BucketSize);
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //This incremental rehash should success because the new size is twice the original
   //and split_count is the same as the old bucket count
   BOOST_TEST(testset1.incremental_rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
                     pointer_to(buckets2[0]), BucketSize*2)) == true);
   BOOST_TEST(testset1.split_count() == BucketSize);
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //This incremental rehash should also success because the new size is half the original
   //and split_count is the same as the new bucket count
   BOOST_TEST(testset1.incremental_rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
                           pointer_to(buckets1[0]), BucketSize)) == true);
   BOOST_TEST(testset1.split_count() == BucketSize);
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Shrink rehash
   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
         pointer_to(buckets1[0]), 4));
   BOOST_TEST (testset1.incremental_rehash() == false);
   {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Shrink rehash again
   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
         pointer_to(buckets1[0]), 2));
   BOOST_TEST (testset1.incremental_rehash() == false);
   {  int init_values [] = { 2, 2, 4, 3, 5, 1 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Growing rehash
   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::
         pointer_to(buckets1[0]), BucketSize));

   //Full rehash (no effects)
   testset1.full_rehash();
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Incremental rehash shrinking
   //First incremental rehashes should lead to the same sequence
   for(std::size_t split_bucket = testset1.split_count(); split_bucket > 6; --split_bucket){
      BOOST_TEST (testset1.incremental_rehash(false) == true);
      BOOST_TEST(testset1.split_count() == (split_bucket-1));
      {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
   }

   //Incremental rehash step
   BOOST_TEST (testset1.incremental_rehash(false) == true);
   BOOST_TEST(testset1.split_count() == (BucketSize/2+1));
   {  int init_values [] = { 5, 1, 2, 2, 3, 4 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Incremental rehash step 2
   BOOST_TEST (testset1.incremental_rehash(false) == true);
   BOOST_TEST(testset1.split_count() == (BucketSize/2));
   {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //This incremental rehash should fail because we've reached the half of the bucket array
   BOOST_TEST(testset1.incremental_rehash(false) == false);
   BOOST_TEST(testset1.split_count() == BucketSize/2);
   {  int init_values [] = { 4, 5, 1, 2, 2, 3 };
   TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
}
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_rehash(value_cont_type& values, detail::false_)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;

   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;

   typename unordered_type::bucket_type buckets1 [BucketSize];
   typename unordered_type::bucket_type buckets2 [2];
   typename unordered_type::bucket_type buckets3 [BucketSize*2];

   unordered_type testset1(&values[0], &values[0] + 6, bucket_traits(
      pointer_traits<bucket_ptr>::
         pointer_to(buckets1[0]), BucketSize));
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets2[0]), 2));
   {  int init_values [] = { 4, 2, 2, 5, 3, 1 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets3[0]), BucketSize*2));
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Now rehash reducing the buckets
   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets3[0]), 2));
   {  int init_values [] = { 4, 2, 2, 5, 3, 1 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Now rehash increasing the buckets
   testset1.rehash(bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets3[0]), BucketSize*2));
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }

   //Full rehash (no effects)
   testset1.full_rehash();
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE( init_values, testset1 );  }
}

//test: find, equal_range (lower_bound, upper_bound):
template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_find(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;
   typedef typename unordered_type::value_type value_type;

   typedef typename unordered_type::bucket_traits  bucket_traits;
   typedef typename unordered_type::bucket_ptr     bucket_ptr;
   typedef typename unordered_type::key_of_value   key_of_value;
   const bool is_multikey = boost::intrusive::test::is_multikey_true<unordered_type>::value;

   typename unordered_type::bucket_type buckets[BucketSize];
   unordered_type testset(values.begin(), values.end(), bucket_traits(
      pointer_traits<bucket_ptr>::pointer_to(buckets[0]), BucketSize));

   typedef typename unordered_type::iterator iterator;

   value_type cmp_val;
   cmp_val.value_ = 2;
   BOOST_TEST (testset.count(key_of_value()(cmp_val)) == (is_multikey ? 2 : 1));
   iterator i = testset.find (key_of_value()(cmp_val));
   BOOST_TEST (i->value_ == 2);
   if(is_multikey)
      BOOST_TEST ((++i)->value_ == 2);
   else
      BOOST_TEST ((++i)->value_ != 2);
   std::pair<iterator,iterator> range = testset.equal_range (key_of_value()(cmp_val));

   BOOST_TEST (range.first->value_ == 2);
   BOOST_TEST (range.second->value_ == 3);
   BOOST_TEST (boost::intrusive::iterator_distance (range.first, range.second) == (is_multikey ? 2 : 1));
   cmp_val.value_ = 7;
   BOOST_TEST (testset.find (key_of_value()(cmp_val)) == testset.end());
   BOOST_TEST (testset.count(key_of_value()(cmp_val)) == 0);
}


template<class ContainerDefiner>
void test_unordered<ContainerDefiner>::test_clone(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type unordered_type;
   typedef typename unordered_type::value_type value_type;
   typedef std::multiset<value_type> std_multiset_t;

   typedef typename unordered_type::bucket_traits bucket_traits;
   typedef typename unordered_type::bucket_ptr    bucket_ptr;

   {
      //Test with equal bucket arrays
      typename unordered_type::bucket_type buckets1 [BucketSize];
      typename unordered_type::bucket_type buckets2 [BucketSize];
      unordered_type testset1 (values.begin(), values.end(), bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets1[0]), BucketSize));
      unordered_type testset2 (bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets2[0]), BucketSize));

      testset2.clone_from(testset1, test::new_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guarantee in the cloning so insert data in a set and test
      std_multiset_t src(testset1.begin(), testset1.end());
      std_multiset_t dst(testset2.begin(), testset2.end());
      BOOST_TEST (src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());

      testset2.clone_from(boost::move(testset1), test::new_nonconst_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guarantee in the cloning so insert data in a set and test
      std_multiset_t(testset1.begin(), testset1.end()).swap(src);
      std_multiset_t(testset2.begin(), testset2.end()).swap(dst);
      BOOST_TEST(src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());
   }
   {
      //Test with bigger source bucket arrays
      typename unordered_type::bucket_type buckets1 [BucketSize*2];
      typename unordered_type::bucket_type buckets2 [BucketSize];
      unordered_type testset1 (values.begin(), values.end(), bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets1[0]), BucketSize*2));
      unordered_type testset2 (bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets2[0]), BucketSize));

      testset2.clone_from(testset1, test::new_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guarantee in the cloning so insert data in a set and test
      std_multiset_t src(testset1.begin(), testset1.end());
      std_multiset_t dst(testset2.begin(), testset2.end());
      BOOST_TEST (src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());

      testset2.clone_from(boost::move(testset1), test::new_nonconst_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guarantee in the cloning so insert data in a set and test
      std_multiset_t(testset1.begin(), testset1.end()).swap(src);
      std_multiset_t(testset2.begin(), testset2.end()).swap(dst);
      BOOST_TEST (src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());
   }
   {
      //Test with smaller source bucket arrays
      typename unordered_type::bucket_type buckets1 [BucketSize];
      typename unordered_type::bucket_type buckets2 [BucketSize*2];
      unordered_type testset1 (values.begin(), values.end(), bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets1[0]), BucketSize));
      unordered_type testset2 (bucket_traits(
         pointer_traits<bucket_ptr>::pointer_to(buckets2[0]), BucketSize*2));

      testset2.clone_from(testset1, test::new_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guaranteed in the cloning so insert data in a set and test
      std_multiset_t src(testset1.begin(), testset1.end());
      std_multiset_t dst(testset2.begin(), testset2.end());
      BOOST_TEST (src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());

      testset2.clone_from(boost::move(testset1), test::new_nonconst_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST(testset1 == testset2);
      //Ordering is not guaranteed in the cloning so insert data in a set and test
      std_multiset_t(testset1.begin(), testset1.end()).swap(src);
      std_multiset_t(testset2.begin(), testset2.end()).swap(dst);
      BOOST_TEST (src.size() == dst.size() && std::equal(src.begin(), src.end(), dst.begin()));
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());
   }
}

}  //namespace test{
}  //namespace intrusive{
}  //namespace boost{
