//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_TEST_NAMED_RESOURCE_TEMPLATE_HEADER
#define BOOST_INTERPROCESS_TEST_NAMED_RESOURCE_TEMPLATE_HEADER

#include <boost/interprocess/detail/config_begin.hpp>
#include <boost/interprocess/exceptions.hpp>
#include "boost_interprocess_check.hpp"
#include "get_process_id_name.hpp"
#include <iostream>
#include <typeinfo>
#include <boost/interprocess/creation_tags.hpp>

namespace boost { namespace interprocess { namespace test {

template <class NamedResource>
inline void create_then_open_then_open_or_create()
{
   BOOST_TRY{
      //Create it and open it twice
      NamedResource nresource1(create_only);
      NamedResource nresource2(open_only);
      NamedResource nresource3(open_or_create);
   }
   BOOST_CATCH(...){
      //This shouldn't throw so show the error
      BOOST_INTERPROCESS_CHECK( false );
   } BOOST_CATCH_END
}

template <class NamedResource>
inline void open_or_create_then_create()
{
   //Create it with open_or_create and try to create it twice
   NamedResource nresource1(open_or_create);
   BOOST_TRY{
      NamedResource nresource2(create_only);
   }
   BOOST_CATCH(interprocess_exception &err){
      BOOST_INTERPROCESS_CHECK(err.get_error_code() == already_exists_error);
   } BOOST_CATCH_END
}

template <class NamedResource>
inline void dont_create_and_open()
{
   //Try to open it without creating
   BOOST_TRY{
      NamedResource nresource1(open_only);
   }
   BOOST_CATCH(interprocess_exception &err){
      BOOST_INTERPROCESS_CHECK(err.get_error_code() == not_found_error);
      return;
   } BOOST_CATCH_END
   //The mutex should not exist
   BOOST_INTERPROCESS_CHECK(false);
}

template <class NamedResource>
inline void test_named_creation()
{
   std::cout   << "create_then_open_then_open_or_create<"
               << typeid(NamedResource).name() << ">" << std::endl;
   create_then_open_then_open_or_create<NamedResource>();
   std::cout   << "open_or_create_then_create<"
               << typeid(NamedResource).name() << ">" << std::endl;
   open_or_create_then_create<NamedResource>();
   std::cout   << "dont_create_and_open<"
               << typeid(NamedResource).name() << ">" << std::endl;
   dont_create_and_open<NamedResource>();
}

template<class NamedSync>
class named_sync_wrapper
   : public NamedSync
{
   public:
   named_sync_wrapper()
      :  NamedSync(open_or_create, test::get_process_id_ptr_name(this))
   {}

   ~named_sync_wrapper()
   {
      NamedSync::remove(test::get_process_id_ptr_name(this));
   }
};

template<class NamedSync>
struct named_sync_deleter
{
   ~named_sync_deleter()
   {  NamedSync::remove(test::get_process_id_name()); }
};

#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

template<class NamedSync>
struct named_sync_deleter_w
{
   ~named_sync_deleter_w()
   {  NamedSync::remove(test::get_process_id_wname()); }
};

#endif   //#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)


//This wrapper is necessary to have a common constructor
//in generic named_creation_template functions
template<class NamedSync>
class named_sync_creation_test_wrapper
   : public test::named_sync_deleter<NamedSync>, public NamedSync
{
   public:
   named_sync_creation_test_wrapper(create_only_t)
      :  NamedSync(create_only, test::get_process_id_name())
   {}

   named_sync_creation_test_wrapper(open_only_t)
      :  NamedSync(open_only, test::get_process_id_name())
   {}

   named_sync_creation_test_wrapper(open_or_create_t)
      :  NamedSync(open_or_create, test::get_process_id_name())
   {}

   ~named_sync_creation_test_wrapper()
   {}
};

#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

//This wrapper is necessary to have a common constructor
//in generic named_creation_template functions
template<class NamedSync>
class named_sync_creation_test_wrapper_w
   : public test::named_sync_deleter_w<NamedSync>, public NamedSync
{
   public:
   named_sync_creation_test_wrapper_w(create_only_t)
      :  NamedSync(create_only, test::get_process_id_wname())
   {}

   named_sync_creation_test_wrapper_w(open_only_t)
      :  NamedSync(open_only, test::get_process_id_wname())
   {}

   named_sync_creation_test_wrapper_w(open_or_create_t)
      :  NamedSync(open_or_create, test::get_process_id_wname())
   {}

   ~named_sync_creation_test_wrapper_w()
   {}
};

#endif   //#if defined(BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES)

}}}   //namespace boost { namespace interprocess { namespace test {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_TEST_NAMED_RESOURCE_TEMPLATE_HEADER
