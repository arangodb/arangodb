// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_INSERT_GUARD_HPP
#define TAO_JSON_PEGTL_ANALYSIS_INSERT_GUARD_HPP

#include <utility>

#include "../config.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   template< typename C >
   class insert_guard
   {
   public:
      insert_guard( C& container, const typename C::value_type& value )
         : m_i( container.insert( value ) ),
           m_c( container )
      {
      }

      insert_guard( const insert_guard& ) = delete;
      insert_guard( insert_guard&& ) = delete;

      ~insert_guard()
      {
         if( m_i.second ) {
            m_c.erase( m_i.first );
         }
      }

      void operator=( const insert_guard& ) = delete;
      void operator=( insert_guard&& ) = delete;

      explicit operator bool() const noexcept
      {
         return m_i.second;
      }

   private:
      const std::pair< typename C::iterator, bool > m_i;
      C& m_c;
   };

   template< typename C >
   insert_guard( C&, const typename C::value_type& )->insert_guard< C >;

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
