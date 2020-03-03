// Copyright (c) 2016-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_INTERNAL_ISTREAM_READER_HPP
#define TAO_JSON_PEGTL_INTERNAL_ISTREAM_READER_HPP

#include <istream>
#include <system_error>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::internal
{
   struct istream_reader
   {
      explicit istream_reader( std::istream& s ) noexcept
         : m_istream( s )
      {
      }

      [[nodiscard]] std::size_t operator()( char* buffer, const std::size_t length )
      {
         m_istream.read( buffer, std::streamsize( length ) );

         if( const auto r = m_istream.gcount() ) {
            return std::size_t( r );
         }
         if( m_istream.eof() ) {
            return 0;
         }
         const auto ec = errno;
         throw std::system_error( ec, std::system_category(), "std::istream::read() failed" );
      }

      std::istream& m_istream;
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::internal

#endif
