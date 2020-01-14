// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_FILE_OPENER_HPP
#define TAO_JSON_PEGTL_INTERNAL_FILE_OPENER_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <system_error>
#include <utility>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct file_opener
   {
      explicit file_opener( const char* filename )
         : m_source( filename ),
           m_fd( open() )
      {
      }

      file_opener( const file_opener& ) = delete;
      file_opener( file_opener&& ) = delete;

      ~file_opener() noexcept
      {
         ::close( m_fd );
      }

      void operator=( const file_opener& ) = delete;
      void operator=( file_opener&& ) = delete;

      [[nodiscard]] std::size_t size() const
      {
         struct stat st;
         errno = 0;
         if( ::fstat( m_fd, &st ) < 0 ) {
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), m_source );
         }
         return std::size_t( st.st_size );
      }

      const char* const m_source;
      const int m_fd;

   private:
      [[nodiscard]] int open() const
      {
         errno = 0;
         const int fd = ::open( m_source,
                                O_RDONLY
#if defined( O_CLOEXEC )
                                   | O_CLOEXEC
#endif
         );
         if( fd >= 0 ) {
            return fd;
         }
         const auto ec = errno;
         throw std::system_error( ec, std::system_category(), m_source );
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
