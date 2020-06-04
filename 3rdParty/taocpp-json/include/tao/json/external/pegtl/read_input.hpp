// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_READ_INPUT_HPP
#define TAO_JSON_PEGTL_READ_INPUT_HPP

#include <string>

#include "config.hpp"
#include "eol.hpp"
#include "string_input.hpp"
#include "tracking_mode.hpp"

#include "internal/file_reader.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace internal
   {
      struct filename_holder
      {
         const std::string filename;

         template< typename T >
         explicit filename_holder( T&& in_filename )
            : filename( std::forward< T >( in_filename ) )
         {
         }

         filename_holder( const filename_holder& ) = delete;
         filename_holder( filename_holder&& ) = delete;

         ~filename_holder() = default;

         void operator=( const filename_holder& ) = delete;
         void operator=( filename_holder&& ) = delete;
      };

   }  // namespace internal

   template< tracking_mode P = tracking_mode::eager, typename Eol = eol::lf_crlf >
   struct read_input
      : private internal::filename_holder,
        public string_input< P, Eol, const char* >
   {
      template< typename T >
      explicit read_input( T&& in_filename )
         : internal::filename_holder( std::forward< T >( in_filename ) ),
           string_input< P, Eol, const char* >( internal::file_reader( filename.c_str() ).read(), filename.c_str() )
      {
      }

      template< typename T >
      read_input( FILE* in_file, T&& in_filename )
         : internal::filename_holder( std::forward< T >( in_filename ) ),
           string_input< P, Eol, const char* >( internal::file_reader( in_file, filename.c_str() ).read(), filename.c_str() )
      {
      }

      read_input( const read_input& ) = delete;
      read_input( read_input&& ) = delete;

      ~read_input() = default;

      void operator=( const read_input& ) = delete;
      void operator=( read_input&& ) = delete;
   };

   template< typename... Ts >
   explicit read_input( Ts&&... )->read_input<>;

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
