// Copyright (c) 2014-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_JSON_PEGTL_ANALYSIS_ANALYZE_CYCLES_HPP
#define TAO_JSON_PEGTL_ANALYSIS_ANALYZE_CYCLES_HPP

#include <cassert>

#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

#include <iostream>
#include <utility>

#include "../config.hpp"

#include "grammar_info.hpp"
#include "insert_guard.hpp"

namespace TAO_JSON_PEGTL_NAMESPACE::analysis
{
   struct analyze_cycles_impl
   {
   protected:
      explicit analyze_cycles_impl( const bool verbose ) noexcept
         : m_verbose( verbose ),
           m_problems( 0 )
      {
      }

      const bool m_verbose;
      unsigned m_problems;
      grammar_info m_info;
      std::set< std::string_view > m_stack;
      std::map< std::string_view, bool > m_cache;
      std::map< std::string_view, bool > m_results;

      [[nodiscard]] std::map< std::string_view, rule_info >::const_iterator find( const std::string_view name ) const noexcept
      {
         const auto iter = m_info.map.find( name );
         assert( iter != m_info.map.end() );
         return iter;
      }

      [[nodiscard]] bool work( const std::map< std::string_view, rule_info >::const_iterator& start, const bool accum )
      {
         const auto j = m_cache.find( start->first );

         if( j != m_cache.end() ) {
            return j->second;
         }
         if( const auto g = insert_guard( m_stack, start->first ) ) {
            switch( start->second.type ) {
               case rule_type::any: {
                  bool a = false;
                  for( const auto& r : start->second.rules ) {
                     a = a || work( find( r ), accum || a );
                  }
                  return m_cache[ start->first ] = true;
               }
               case rule_type::opt: {
                  bool a = false;
                  for( const auto& r : start->second.rules ) {
                     a = a || work( find( r ), accum || a );
                  }
                  return m_cache[ start->first ] = false;
               }
               case rule_type::seq: {
                  bool a = false;
                  for( const auto& r : start->second.rules ) {
                     a = a || work( find( r ), accum || a );
                  }
                  return m_cache[ start->first ] = a;
               }
               case rule_type::sor: {
                  bool a = true;
                  for( const auto& r : start->second.rules ) {
                     a = a && work( find( r ), accum );
                  }
                  return m_cache[ start->first ] = a;
               }
            }
            throw std::logic_error( "code should be unreachable: invalid rule_type value" );  // LCOV_EXCL_LINE
         }
         if( !accum ) {
            ++m_problems;
            if( m_verbose ) {
               std::cout << "problem: cycle without progress detected at rule class " << start->first << std::endl;  // LCOV_EXCL_LINE
            }
         }
         return m_cache[ start->first ] = accum;
      }
   };

   template< typename Grammar >
   struct analyze_cycles
      : private analyze_cycles_impl
   {
      explicit analyze_cycles( const bool verbose )
         : analyze_cycles_impl( verbose )
      {
         Grammar::analyze_t::template insert< Grammar >( m_info );
      }

      [[nodiscard]] std::size_t problems()
      {
         for( auto i = m_info.map.begin(); i != m_info.map.end(); ++i ) {
            m_results[ i->first ] = work( i, false );
            m_cache.clear();
         }
         return m_problems;
      }

      template< typename Rule >
      [[nodiscard]] bool consumes() const noexcept
      {
         const auto i = m_results.find( internal::demangle< Rule >() );
         assert( i != m_results.end() );
         return i->second;
      }
   };

}  // namespace TAO_JSON_PEGTL_NAMESPACE::analysis

#endif
