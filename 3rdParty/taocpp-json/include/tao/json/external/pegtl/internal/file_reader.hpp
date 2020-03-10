// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_FILE_READER_HPP
#define TAO_JSON_PEGTL_INTERNAL_FILE_READER_HPP

#include <cstdio>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   [[nodiscard]] inline std::FILE* file_open( const char* filename )
   {
      errno = 0;
#if defined( _MSC_VER )
      std::FILE* file;
      if( ::fopen_s( &file, filename, "rb" ) == 0 )
#elif defined( __MINGW32__ )
      if( auto* file = std::fopen( filename, "rb" ) )
#else
      if( auto* file = std::fopen( filename, "rbe" ) )
#endif
      {
         return file;
      }
      const auto ec = errno;
      throw std::system_error( ec, std::system_category(), filename );
   }

   struct file_close
   {
      void operator()( FILE* f ) const noexcept
      {
         std::fclose( f );
      }
   };

   class file_reader
   {
   public:
      explicit file_reader( const char* filename )
         : m_source( filename ),
           m_file( file_open( m_source ) )
      {
      }

      file_reader( FILE* file, const char* filename ) noexcept
         : m_source( filename ),
           m_file( file )
      {
      }

      file_reader( const file_reader& ) = delete;
      file_reader( file_reader&& ) = delete;

      ~file_reader() = default;

      void operator=( const file_reader& ) = delete;
      void operator=( file_reader&& ) = delete;

      [[nodiscard]] std::size_t size() const
      {
         errno = 0;
         if( std::fseek( m_file.get(), 0, SEEK_END ) != 0 ) {
            // LCOV_EXCL_START
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), m_source );
            // LCOV_EXCL_STOP
         }
         errno = 0;
         const auto s = std::ftell( m_file.get() );
         if( s < 0 ) {
            // LCOV_EXCL_START
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), m_source );
            // LCOV_EXCL_STOP
         }
         errno = 0;
         if( std::fseek( m_file.get(), 0, SEEK_SET ) != 0 ) {
            // LCOV_EXCL_START
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), m_source );
            // LCOV_EXCL_STOP
         }
         return std::size_t( s );
      }

      [[nodiscard]] std::string read() const
      {
         std::string nrv;
         nrv.resize( size() );
         errno = 0;
         if( !nrv.empty() && ( std::fread( &nrv[ 0 ], nrv.size(), 1, m_file.get() ) != 1 ) ) {
            // LCOV_EXCL_START
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), m_source );
            // LCOV_EXCL_STOP
         }
         return nrv;
      }

   private:
      const char* const m_source;
      const std::unique_ptr< std::FILE, file_close > m_file;
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
