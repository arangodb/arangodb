// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_MMAP_INPUT_HPP
#define TAO_JSON_PEGTL_MMAP_INPUT_HPP

#include <string>
#include <utility>

#include "config.hpp"
#include "eol.hpp"
#include "memory_input.hpp"
#include "tracking_mode.hpp"

#if defined( __unix__ ) || ( defined( __APPLE__ ) && defined( __MACH__ ) )
#include <unistd.h>  // Required for _POSIX_MAPPED_FILES
#endif

#if defined( _POSIX_MAPPED_FILES )
#include "internal/file_mapper_posix.hpp"
#elif defined( _WIN32 )
#include "internal/file_mapper_win32.hpp"
#else
#endif

namespace TAO_JSON_PEGTL_NAMESPACE
{
   namespace internal
   {
      struct mmap_holder
      {
         const std::string filename;
         const file_mapper data;

         template< typename T >
         explicit mmap_holder( T&& in_filename )
            : filename( std::forward< T >( in_filename ) ),
              data( filename.c_str() )
         {
         }

         mmap_holder( const mmap_holder& ) = delete;
         mmap_holder( mmap_holder&& ) = delete;

         ~mmap_holder() = default;

         void operator=( const mmap_holder& ) = delete;
         void operator=( mmap_holder&& ) = delete;
      };

   }  // namespace internal

   template< tracking_mode P = tracking_mode::eager, typename Eol = eol::lf_crlf >
   struct mmap_input
      : private internal::mmap_holder,
        public memory_input< P, Eol, const char* >
   {
      template< typename T >
      explicit mmap_input( T&& in_filename )
         : internal::mmap_holder( std::forward< T >( in_filename ) ),
           memory_input< P, Eol, const char* >( data.begin(), data.end(), filename.c_str() )
      {
      }

      mmap_input( const mmap_input& ) = delete;
      mmap_input( mmap_input&& ) = delete;

      ~mmap_input() = default;

      void operator=( const mmap_input& ) = delete;
      void operator=( mmap_input&& ) = delete;
   };

   template< typename... Ts >
   explicit mmap_input( Ts&&... )->mmap_input<>;

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
