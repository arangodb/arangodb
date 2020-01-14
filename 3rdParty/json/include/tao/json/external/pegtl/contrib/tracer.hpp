// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_CONTRIB_TRACER_HPP
#define TAO_JSON_PEGTL_CONTRIB_TRACER_HPP

#include <cassert>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

#include "../config.hpp"
#include "../normal.hpp"

#include "../internal/demangle.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace internal
   {
      template< typename Input >
      void print_current( const Input& in )
      {
         if( in.empty() ) {
            std::cerr << "<eof>";
         }
         else {
            const auto c = in.peek_uint8();
            switch( c ) {
               case 0:
                  std::cerr << "<nul> = ";
                  break;
               case 9:
                  std::cerr << "<ht> = ";
                  break;
               case 10:
                  std::cerr << "<lf> = ";
                  break;
               case 13:
                  std::cerr << "<cr> = ";
                  break;
               default:
                  if( isprint( c ) ) {
                     std::cerr << '\'' << c << "' = ";
                  }
            }
            std::cerr << "(char)" << unsigned( c );
         }
      }

   }  // namespace internal

   struct trace_state
   {
      unsigned rule = 0;
      unsigned line = 0;
      std::vector< unsigned > stack;
   };

   template< template< typename... > class Base >
   struct trace
   {
      template< typename Rule >
      struct control
         : Base< Rule >
      {
         template< typename Input, typename... States >
         static void start( const Input& in, States&&... st )
         {
            std::cerr << in.position() << "  start  " << internal::demangle< Rule >() << "; current ";
            print_current( in );
            std::cerr << std::endl;
            Base< Rule >::start( in, st... );
         }

         template< typename Input, typename... States >
         static void start( const Input& in, trace_state& ts, States&&... st )
         {
            std::cerr << std::setw( 6 ) << ++ts.line << " " << std::setw( 6 ) << ++ts.rule << " ";
            start( in, st... );
            ts.stack.push_back( ts.rule );
         }

         template< typename Input, typename... States >
         static void success( const Input& in, States&&... st )
         {
            std::cerr << in.position() << " success " << internal::demangle< Rule >() << "; next ";
            print_current( in );
            std::cerr << std::endl;
            Base< Rule >::success( in, st... );
         }

         template< typename Input, typename... States >
         static void success( const Input& in, trace_state& ts, States&&... st )
         {
            assert( !ts.stack.empty() );
            std::cerr << std::setw( 6 ) << ++ts.line << " " << std::setw( 6 ) << ts.stack.back() << " ";
            success( in, st... );
            ts.stack.pop_back();
         }

         template< typename Input, typename... States >
         static void failure( const Input& in, States&&... st )
         {
            std::cerr << in.position() << " failure " << internal::demangle< Rule >() << std::endl;
            Base< Rule >::failure( in, st... );
         }

         template< typename Input, typename... States >
         static void failure( const Input& in, trace_state& ts, States&&... st )
         {
            assert( !ts.stack.empty() );
            std::cerr << std::setw( 6 ) << ++ts.line << " " << std::setw( 6 ) << ts.stack.back() << " ";
            failure( in, st... );
            ts.stack.pop_back();
         }

         template< template< typename... > class Action, typename Iterator, typename Input, typename... States >
         static auto apply( const Iterator& begin, const Input& in, States&&... st )
            -> decltype( Base< Rule >::template apply< Action >( begin, in, st... ) )
         {
            std::cerr << in.position() << "  apply  " << internal::demangle< Rule >() << std::endl;
            return Base< Rule >::template apply< Action >( begin, in, st... );
         }

         template< template< typename... > class Action, typename Iterator, typename Input, typename... States >
         static auto apply( const Iterator& begin, const Input& in, trace_state& ts, States&&... st )
            -> decltype( apply< Action >( begin, in, st... ) )
         {
            std::cerr << std::setw( 6 ) << ++ts.line << "        ";
            return apply< Action >( begin, in, st... );
         }

         template< template< typename... > class Action, typename Input, typename... States >
         static auto apply0( const Input& in, States&&... st )
            -> decltype( Base< Rule >::template apply0< Action >( in, st... ) )
         {
            std::cerr << in.position() << "  apply0 " << internal::demangle< Rule >() << std::endl;
            return Base< Rule >::template apply0< Action >( in, st... );
         }

         template< template< typename... > class Action, typename Input, typename... States >
         static auto apply0( const Input& in, trace_state& ts, States&&... st )
            -> decltype( apply0< Action >( in, st... ) )
         {
            std::cerr << std::setw( 6 ) << ++ts.line << "        ";
            return apply0< Action >( in, st... );
         }
      };
   };

   template< typename Rule >
   using tracer = trace< normal >::control< Rule >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
