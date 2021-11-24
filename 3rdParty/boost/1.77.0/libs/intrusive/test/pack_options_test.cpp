//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/intrusive/pack_options.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/static_assert.hpp>

struct empty_default{};

using namespace boost::intrusive;

//Test BOOST_INTRUSIVE_OPTION_CONSTANT
BOOST_INTRUSIVE_OPTION_CONSTANT(incremental, bool, Enabled, is_incremental)
const bool is_incremental_value = pack_options< empty_default, incremental<true> >::type::is_incremental;
BOOST_STATIC_ASSERT(( is_incremental_value == true ));

//Test BOOST_INTRUSIVE_OPTION_TYPE
BOOST_INTRUSIVE_OPTION_TYPE(my_pointer, VoidPointer, typename boost::intrusive::detail::remove_pointer<VoidPointer>::type, my_pointer_type)
typedef pack_options< empty_default, my_pointer<void*> >::type::my_pointer_type my_pointer_type;
BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<my_pointer_type, void>::value ));

//test combination of BOOST_INTRUSIVE_OPTION_CONSTANT and BOOST_INTRUSIVE_OPTION_TYPE
//    First add new options
struct default_options
{
   static const long long_constant = -3;
   typedef double *  double_typedef;
};

BOOST_INTRUSIVE_OPTION_CONSTANT(incremental2, bool, Enabled, is_incremental2)
BOOST_INTRUSIVE_OPTION_TYPE(my_pointer2, VoidPointer, typename boost::intrusive::detail::add_pointer<VoidPointer>::type, my_pointer_type2)
typedef pack_options < default_options
                     , incremental<false>
                     , my_pointer<float*>
                     , incremental2<true>
                     , my_pointer2<const char*>
                     >::type combined_type;
BOOST_STATIC_ASSERT(( combined_type::is_incremental  == false ));
BOOST_STATIC_ASSERT(( combined_type::is_incremental2 == true  ));
BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<combined_type::my_pointer_type,       float  >::value ));
BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<combined_type::my_pointer_type2, const char**>::value ));

//test packing the default options leads to a default options type
BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<pack_options<default_options>::type, default_options>::value ));

int main()
{
   return 0;
}
