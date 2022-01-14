//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2021-2021. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_TEST_NAMED_CONDITION_HELPERS_HEADER
#define BOOST_INTERPROCESS_TEST_NAMED_CONDITION_HELPERS_HEADER

#include <boost/interprocess/detail/config_begin.hpp>

namespace boost { namespace interprocess { namespace test {

template<class NamedCondition>
struct condition_deleter
{
   std::string name;

   ~condition_deleter()
   {
      if(name.empty())
         NamedCondition::remove(test::add_to_process_id_name("named_condition"));
      else
         NamedCondition::remove(name.c_str());
   }
};

#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

template<class NamedCondition>
struct condition_deleter_w
{
   std::string name;

   ~condition_deleter_w()
   {
      if(name.empty())
         NamedCondition::remove(test::add_to_process_id_name(L"named_condition"));
      else
         NamedCondition::remove(name.c_str());
   }
};

#endif   //#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)


inline std::string num_to_string(int n)
{  std::stringstream s; s << n; return s.str(); }

//This wrapper is necessary to have a default constructor
//in generic mutex_test_template functions
template<class NamedCondition>
class named_condition_test_wrapper
   : public condition_deleter<NamedCondition>, public NamedCondition
{
   public:

   named_condition_test_wrapper()
      :  NamedCondition(open_or_create,
             (test::add_to_process_id_name("test_cond") + num_to_string(count)).c_str())
   {
      condition_deleter<NamedCondition>::name += test::add_to_process_id_name("test_cond");
      condition_deleter<NamedCondition>::name += num_to_string(count);
      ++count;
   }

   ~named_condition_test_wrapper()
   {  --count; }


   template <typename L>
   void wait(L& lock)
   {
      ipcdetail::internal_mutex_lock<L> internal_lock(lock);
      NamedCondition::wait(internal_lock);
   }

   template <typename L, typename Pr>
   void wait(L& lock, Pr pred)
   {
      ipcdetail::internal_mutex_lock<L> internal_lock(lock);
      NamedCondition::wait(internal_lock, pred);
   }

   template <typename L, typename TimePoint>
   bool timed_wait(L& lock, const TimePoint &abs_time)
   {
      ipcdetail::internal_mutex_lock<L> internal_lock(lock);
      return NamedCondition::timed_wait(internal_lock, abs_time);
   }

   template <typename L, typename TimePoint, typename Pr>
   bool timed_wait(L& lock, const TimePoint &abs_time, Pr pred)
   {
      ipcdetail::internal_mutex_lock<L> internal_lock(lock);
      return NamedCondition::timed_wait(internal_lock, abs_time, pred);
   }

   static int count;
};

template<class NamedCondition>
int named_condition_test_wrapper<NamedCondition>::count = 0;

//This wrapper is necessary to have a common constructor
//in generic named_creation_template functions
template<class NamedCondition>
class named_condition_creation_test_wrapper
   : public condition_deleter<NamedCondition>, public NamedCondition
{
   public:
   named_condition_creation_test_wrapper(create_only_t)
      :  NamedCondition(create_only, test::add_to_process_id_name("named_condition"))
   {  ++count_;   }

   named_condition_creation_test_wrapper(open_only_t)
      :  NamedCondition(open_only, test::add_to_process_id_name("named_condition"))
   {  ++count_;   }

   named_condition_creation_test_wrapper(open_or_create_t)
      :  NamedCondition(open_or_create, test::add_to_process_id_name("named_condition"))
   {  ++count_;   }

   ~named_condition_creation_test_wrapper()   {
      if(--count_){
         ipcdetail::interprocess_tester::
            dont_close_on_destruction(static_cast<NamedCondition&>(*this));
      }
   }
   static int count_;
};

template<class NamedCondition>
int named_condition_creation_test_wrapper<NamedCondition>::count_ = 0;

#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

//This wrapper is necessary to have a common constructor
//in generic named_creation_template functions
template<class NamedCondition>
class named_condition_creation_test_wrapper_w
   : public condition_deleter_w<NamedCondition>, public NamedCondition
{
   public:
   named_condition_creation_test_wrapper_w(create_only_t)
      :  NamedCondition(create_only, test::add_to_process_id_name(L"named_condition"))
   {  ++count_;   }

   named_condition_creation_test_wrapper_w(open_only_t)
      :  NamedCondition(open_only, test::add_to_process_id_name(L"named_condition"))
   {  ++count_;   }

   named_condition_creation_test_wrapper_w(open_or_create_t)
      :  NamedCondition(open_or_create, test::add_to_process_id_name(L"named_condition"))
   {  ++count_;   }

   ~named_condition_creation_test_wrapper_w()   {
      if(--count_){
         ipcdetail::interprocess_tester::
            dont_close_on_destruction(static_cast<NamedCondition&>(*this));
      }
   }
   static int count_;
};

template<class NamedCondition>
int named_condition_creation_test_wrapper_w<NamedCondition>::count_ = 0;

#endif   //#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

template<class NamedMutex>
struct mutex_deleter
{
   std::string name;

   ~mutex_deleter()
   {
      if(name.empty())
         NamedMutex::remove(test::add_to_process_id_name("named_mutex"));
      else
         NamedMutex::remove(name.c_str());
   }
};

//This wrapper is necessary to have a default constructor
//in generic mutex_test_template functions
template<class NamedMutex>
class named_mutex_test_wrapper
   : public mutex_deleter<NamedMutex>, public NamedMutex
{
   public:
   named_mutex_test_wrapper()
      :  NamedMutex(open_or_create,
             (test::add_to_process_id_name("test_mutex") + num_to_string(count)).c_str())
   {
      mutex_deleter<NamedMutex>::name += test::add_to_process_id_name("test_mutex");
      mutex_deleter<NamedMutex>::name += num_to_string(count);
      ++count;
   }

   typedef NamedMutex internal_mutex_type;

   internal_mutex_type &internal_mutex()
   {  return *this;  }

   ~named_mutex_test_wrapper()
   {  --count; }

   static int count;
};

template<class NamedMutex>
int named_mutex_test_wrapper<NamedMutex>::count = 0;

template<class NamedCondition, class NamedMutex>
int test_named_condition()
{
   int ret = 0;
   try{
      //Remove previous mutexes and conditions
      NamedMutex::remove(test::add_to_process_id_name("test_mutex0"));
      NamedCondition::remove(test::add_to_process_id_name("test_cond0"));
      NamedCondition::remove(test::add_to_process_id_name("test_cond1"));
      NamedCondition::remove(test::add_to_process_id_name("named_condition"));
      NamedMutex::remove(test::add_to_process_id_name("named_mutex"));

      test::test_named_creation<test::named_condition_creation_test_wrapper<NamedCondition> >();
      #if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)
      //Remove previous mutexes and conditions
      NamedMutex::remove(test::add_to_process_id_name("test_mutex0"));
      NamedCondition::remove(test::add_to_process_id_name("test_cond0"));
      NamedCondition::remove(test::add_to_process_id_name("test_cond1"));
      NamedCondition::remove(test::add_to_process_id_name("named_condition"));
      NamedMutex::remove(test::add_to_process_id_name("named_mutex"));
      test::test_named_creation<test::named_condition_creation_test_wrapper_w<NamedCondition> >();
      #endif
      test::do_test_condition<test::named_condition_test_wrapper<NamedCondition>
                             ,test::named_mutex_test_wrapper<NamedMutex> >();
   }
   catch(std::exception &ex){
      std::cout << ex.what() << std::endl;
      ret = 1;
   }
   NamedMutex::remove(test::add_to_process_id_name("test_mutex0"));
   NamedCondition::remove(test::add_to_process_id_name("test_cond0"));
   NamedCondition::remove(test::add_to_process_id_name("test_cond1"));
   NamedCondition::remove(test::add_to_process_id_name("named_condition"));
   NamedMutex::remove(test::add_to_process_id_name("named_mutex"));
   return ret;
}

}}}   //namespace boost { namespace interprocess { namespace test {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_TEST_NAMED_CONDITION_HELPERS_HEADER
