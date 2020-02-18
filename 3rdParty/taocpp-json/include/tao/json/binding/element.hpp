// Copyright (c) 2018-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_BINDING_ELEMENT_HPP
#define TAO_JSON_BINDING_ELEMENT_HPP

#include <type_traits>

#include "../consume.hpp"
#include "../forward.hpp"

#include "../events/produce.hpp"

namespace tao::json::binding
{
   namespace internal
   {
      template< typename T, T, typename = void >
      struct element;

      template< typename C, typename T, T C::*P >
      struct element< T C::*, P, std::enable_if_t< std::is_member_object_pointer_v< T C::* > > >
      {
         using class_t = C;
         using value_t = T;

         using internal_t = std::decay_t< decltype( std::declval< C >().*P ) >;

         [[nodiscard]] static const auto& read( const C& v )
         {
            return v.*P;
         }

         template< typename W >
         static void write( C& v, W&& w )
         {
            v.*P = std::forward< W >( w );
         }

         template< template< typename... > class Traits >
         static void to( const basic_value< Traits >& v, C& x )
         {
            v.to( x.*P );
         }

         template< template< typename... > class Traits = traits, typename Producer >
         static void consume( Producer& parser, C& v )
         {
            json::consume< Traits >( parser, v.*P );
         }

         template< template< typename... > class Traits = traits, typename Consumer >
         static void produce( Consumer& consumer, const C& v )
         {
            events::produce< Traits >( consumer, v.*P );
         }
      };

      template< typename C, typename T, T C::*P >
      struct element< T C::*, P, std::enable_if_t< std::is_member_function_pointer_v< T C::* > > >
      {
         using class_t = C;
         using value_t = T;

         using internal_t = std::decay_t< decltype( ( std::declval< const C >().*P )() ) >;

         [[nodiscard]] static decltype( auto ) read( const C& v )
         {
            return ( v.*P )();
         }

         template< template< typename... > class Traits = traits, typename Consumer >
         static void produce( Consumer& consumer, const C& v )
         {
            events::produce< Traits >( consumer, ( v.*P )() );
         }
      };

      template< typename C, typename T, T ( *P )( const C& ) >
      struct element< T ( * )( const C& ), P >
      {
         using class_t = C;
         using value_t = T;

         [[nodiscard]] static decltype( auto ) read( const C& v )
         {
            return P( v );
         }

         template< template< typename... > class Traits = traits, typename Consumer >
         static void produce( Consumer& consumer, const C& v )
         {
            events::produce< Traits >( consumer, P( v ) );
         }
      };

      template< typename CT, CT CP, typename T, T P >
      struct element2;

      template< typename A, typename CR, CR ( *CP )( const A& ) noexcept, typename R, R ( *P )( A& ) noexcept >
      struct element2< CR ( * )( const A& ) noexcept, CP, R ( * )( A& ) noexcept, P >
      {
         using class_t = A;
         using value_t = std::decay_t< R >;

         [[nodiscard]] static decltype( auto ) read( const A& v ) noexcept
         {
            return CP( v );
         }

         template< typename W >
         static void write( A& v, W&& w )
         {
            P( v ) = std::forward< W >( w );
         }

         template< template< typename... > class Traits >
         static void to( const basic_value< Traits >& v, A& x )
         {
            v.to( P( x ) );
         }

         template< template< typename... > class Traits = traits, typename Producer >
         static void consume( Producer& parser, A& v )
         {
            json::consume< Traits >( parser, P( v ) );
         }

         template< template< typename... > class Traits = traits, typename Consumer >
         static void produce( Consumer& consumer, const A& v )
         {
            events::produce< Traits >( consumer, CP( v ) );
         }
      };

      template< typename A, typename CR, CR ( *CP )( const A& ) noexcept, typename R, void ( *P )( A&, R&& ) noexcept >
      struct element2< CR ( * )( const A& ) noexcept, CP, void ( * )( A&, R&& ) noexcept, P >
      {
         using class_t = A;
         using value_t = std::decay_t< R >;

         [[nodiscard]] static decltype( auto ) read( const A& v ) noexcept
         {
            return CP( v );
         }

         template< typename W >
         static void write( A& v, W&& w )
         {
            P( v, std::forward< W >( w ) );
         }

         template< template< typename... > class Traits >
         static void to( const basic_value< Traits >& v, A& x )
         {
            P( x, v.template as< std::decay_t< R > >() );
         }

         template< template< typename... > class Traits = traits, typename Producer >
         static void consume( Producer& parser, A& v )
         {
            P( v, json::consume< Traits >( parser ) );
         }

         template< template< typename... > class Traits = traits, typename Consumer >
         static void produce( Consumer& consumer, const A& v )
         {
            events::produce< Traits >( consumer, CP( v ) );
         }
      };

   }  // namespace internal

   template< auto P >
   using element = internal::element< decltype( P ), P >;

   template< auto CP, auto P >
   using element2 = internal::element2< decltype( CP ), CP, decltype( P ), P >;

}  // namespace tao::json::binding

#endif
