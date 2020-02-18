// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_COUNTER_HPP
#define TAO_JSON_PEGTL_CONTRIB_COUNTER_HPP

#include <map>
#include <string_view>

#include "../config.hpp"
#include "../normal.hpp"

#include "../internal/demangle.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   struct counter_data
   {
      unsigned start = 0;
      unsigned success = 0;
      unsigned failure = 0;
   };

   struct counter_state
   {
      std::map< std::string_view, counter_data > counts;
   };

   template< typename Rule >
   struct counter
      : normal< Rule >
   {
      template< typename Input >
      static void start( const Input& /*unused*/, counter_state& ts )
      {
         ++ts.counts[ internal::demangle< Rule >() ].start;
      }

      template< typename Input >
      static void success( const Input& /*unused*/, counter_state& ts )
      {
         ++ts.counts[ internal::demangle< Rule >() ].success;
      }

      template< typename Input >
      static void failure( const Input& /*unused*/, counter_state& ts )
      {
         ++ts.counts[ internal::demangle< Rule >() ].failure;
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
