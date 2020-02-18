// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_FILE_MAPPER_POSIX_HPP
#define TAO_JSON_PEGTL_INTERNAL_FILE_MAPPER_POSIX_HPP

#include <sys/mman.h>
#include <unistd.h>

#include <system_error>

#include "../config.hpp"

#include "file_opener.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   class file_mapper
   {
   public:
      explicit file_mapper( const char* filename )
         : file_mapper( file_opener( filename ) )
      {
      }

      explicit file_mapper( const file_opener& reader )
         : m_size( reader.size() ),
           m_data( static_cast< const char* >( ::mmap( nullptr, m_size, PROT_READ, MAP_PRIVATE, reader.m_fd, 0 ) ) )
      {
         if( ( m_size != 0 ) && ( intptr_t( m_data ) == -1 ) ) {
            const auto ec = errno;
            throw std::system_error( ec, std::system_category(), reader.m_source );
         }
      }

      file_mapper( const file_mapper& ) = delete;
      file_mapper( file_mapper&& ) = delete;

      ~file_mapper() noexcept
      {
         // Legacy C interface requires pointer-to-mutable but does not write through the pointer.
         ::munmap( const_cast< char* >( m_data ), m_size );
      }

      void operator=( const file_mapper& ) = delete;
      void operator=( file_mapper&& ) = delete;

      [[nodiscard]] bool empty() const noexcept
      {
         return m_size == 0;
      }

      [[nodiscard]] std::size_t size() const noexcept
      {
         return m_size;
      }

      using iterator = const char*;
      using const_iterator = const char*;

      [[nodiscard]] iterator data() const noexcept
      {
         return m_data;
      }

      [[nodiscard]] iterator begin() const noexcept
      {
         return m_data;
      }

      [[nodiscard]] iterator end() const noexcept
      {
         return m_data + m_size;
      }

   private:
      const std::size_t m_size;
      const char* const m_data;
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
