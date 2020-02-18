// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_PARSE_ERROR_HPP
#define TAO_JSON_PEGTL_PARSE_ERROR_HPP

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "position.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE
{
   struct parse_error
      : std::runtime_error
   {
      template< typename Msg >
      parse_error( Msg&& msg, std::vector< position > in_positions )
         : std::runtime_error( std::forward< Msg >( msg ) ),
           positions( std::move( in_positions ) )
      {
      }

      template< typename Msg >
      parse_error( Msg&& msg, const position& pos )
         : std::runtime_error( std::forward< Msg >( msg ) ),
           positions( 1, pos )
      {
      }

      template< typename Msg >
      parse_error( Msg&& msg, position&& pos )
         : std::runtime_error( std::forward< Msg >( msg ) )
      {
         positions.emplace_back( std::move( pos ) );
      }

      template< typename Msg, typename Input >
      parse_error( Msg&& msg, const Input& in )
         : parse_error( std::forward< Msg >( msg ), in.position() )
      {
      }

      std::vector< position > positions;
   };

   inline std::ostream& operator<<( std::ostream& o, const parse_error& e )
   {
      for( auto it = e.positions.rbegin(); it != e.positions.rend(); ++it ) {
         o << *it << ": ";
      }
      return o << e.what();
   }

   [[nodiscard]] inline std::string to_string( const parse_error& e )
   {
      std::ostringstream o;
      o << e;
      return o.str();
   }

}  // namespace TAO_JSON_PEGTL_NAMESPACE

#endif
