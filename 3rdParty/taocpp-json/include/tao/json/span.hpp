// Copyright (c) 2019-2020 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_SPAN_HPP
#define TAO_JSON_SPAN_HPP

#if !defined( TAO_JSON_USE_STD_SPAN ) && ( __cplusplus > 201703L ) && defined( __has_include )
// clang-format off
#if __has_include(<span>)
// clang-format on
#define TAO_JSON_USE_STD_SPAN
#endif
#endif

#if defined( TAO_JSON_USE_STD_SPAN )

#include <span>

namespace tao
{
   using std::dynamic_extent;
   using std::span;

   using std::as_bytes;
   using std::as_writable_bytes;

}  // namespace tao

#else

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>

namespace tao
{
   inline constexpr std::size_t dynamic_extent = std::numeric_limits< std::size_t >::max();

   template< typename ElementType, std::size_t Extent = dynamic_extent >
   class span;

   namespace internal
   {
      template< typename >
      struct is_span_impl
         : std::false_type
      {};

      template< typename ElementType, std::size_t Extent >
      struct is_span_impl< span< ElementType, Extent > >
         : std::true_type
      {};

      template< typename T >
      using is_span = is_span_impl< std::remove_cv_t< T > >;

      template< typename >
      struct is_std_array_impl
         : std::false_type
      {};

      template< typename T, std::size_t N >
      struct is_std_array_impl< std::array< T, N > >
         : std::true_type
      {};

      template< typename T >
      using is_std_array = is_std_array_impl< std::remove_cv_t< T > >;

      template< typename T, typename ElementType >
      using is_span_compatible_ptr = std::is_convertible< T ( * )[], ElementType ( * )[] >;

      template< typename, typename, typename = void >
      struct is_span_compatible_container
         : std::false_type
      {};

      template< typename Container, typename ElementType >
      struct is_span_compatible_container< Container,
                                           ElementType,
                                           std::void_t<
                                              std::enable_if_t< !is_span< Container >::value >,
                                              std::enable_if_t< !is_std_array< Container >::value >,
                                              std::enable_if_t< !std::is_array_v< Container > >,
                                              decltype( std::data( std::declval< Container >() ) ),
                                              decltype( std::size( std::declval< Container >() ) ),
                                              std::enable_if_t< is_span_compatible_ptr< std::remove_pointer_t< decltype( std::data( std::declval< Container& >() ) ) >, ElementType >::value > > >
         : std::true_type
      {};

   }  // namespace internal

   template< typename ElementType, std::size_t Extent >
   class span
   {
   public:
      static_assert( !std::is_abstract_v< ElementType > );

      using element_type = ElementType;
      using value_type = std::remove_cv_t< ElementType >;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using pointer = element_type*;
      using const_pointer = const element_type*;
      using reference = element_type&;
      using const_reference = const element_type&;
      using iterator = pointer;
      using reverse_iterator = std::reverse_iterator< iterator >;

      static constexpr size_type extent = Extent;

      template< typename T = void, typename = std::enable_if_t< Extent == 0, T > >
      constexpr span() noexcept
         : m_data( nullptr )
      {}

      constexpr span( pointer ptr, size_type count ) noexcept
         : m_data( ptr )
      {
         assert( count == Extent );
      }

      constexpr span( pointer first, pointer last ) noexcept
         : m_data( first )
      {
         assert( std::distance( first, last ) == Extent );
      }

      constexpr span( element_type ( &arr )[ Extent ] ) noexcept
         : m_data( arr )
      {}

      template< std::size_t N, typename = std::enable_if_t< ( N == Extent ) && tao::internal::is_span_compatible_ptr< value_type, ElementType >::value > >
      constexpr span( std::array< value_type, N >& arr ) noexcept
         : m_data( arr.data() )
      {}

      template< std::size_t N, typename = std::enable_if_t< ( N == Extent ) && tao::internal::is_span_compatible_ptr< const value_type, ElementType >::value > >
      constexpr span( const std::array< value_type, N >& arr ) noexcept
         : m_data( arr.data() )
      {}

      constexpr span( const span& other ) = default;

      template< typename OtherElementType, typename = std::enable_if_t< tao::internal::is_span_compatible_ptr< OtherElementType, ElementType >::value > >
      constexpr span( const span< OtherElementType, Extent >& s ) noexcept
         : m_data( s.data() )
      {}

      ~span() = default;

      constexpr span& operator=( const span& ) = default;

      constexpr size_type size() const noexcept
      {
         return Extent;
      }

      constexpr size_type size_bytes() const noexcept
      {
         return Extent * sizeof( element_type );
      }

      [[nodiscard]] constexpr bool empty() const noexcept
      {
         return Extent == 0;
      }

      constexpr reference operator[]( size_type idx ) const noexcept
      {
         assert( idx < Extent );
         return *( data() + idx );
      }

      constexpr reference front() const noexcept
      {
         assert( Extent != 0 );
         return *data();
      }

      constexpr reference back() const noexcept
      {
         assert( Extent != 0 );
         return *( data() + ( Extent - 1 ) );
      }

      constexpr pointer data() const noexcept
      {
         return m_data;
      }

      constexpr iterator begin() const noexcept
      {
         return data();
      }

      constexpr iterator end() const noexcept
      {
         return data() + Extent;
      }

      constexpr reverse_iterator rbegin() const noexcept
      {
         return reverse_iterator( end() );
      }

      constexpr reverse_iterator rend() const noexcept
      {
         return reverse_iterator( begin() );
      }

      friend constexpr iterator begin( span s ) noexcept
      {
         return s.begin();
      }

      friend constexpr iterator end( span s ) noexcept
      {
         return s.end();
      }

      template< std::size_t Count >
      constexpr span< element_type, Count > first() const noexcept
      {
         static_assert( Count <= Extent );
         return { data(), Count };
      }

      template< std::size_t Count >
      constexpr span< element_type, Count > last() const noexcept
      {
         static_assert( Count <= Extent );
         return { data() + ( Extent - Count ), Count };
      }

      template< std::size_t Offset, std::size_t Count = dynamic_extent >
      constexpr auto subspan() const
         -> span< element_type, ( ( Count != dynamic_extent ) ? Count : ( Extent - Offset ) ) >
      {
         static_assert( Offset <= Extent );
         static_assert( ( Count == dynamic_extent ) || ( Count <= ( Extent - Offset ) ) );
         return { data() + Offset, ( Count != dynamic_extent ) ? Count : ( Extent - Offset ) };
      }

      constexpr span< element_type, dynamic_extent > first( size_type count ) const
      {
         assert( count <= Extent );
         return { data(), count };
      }

      constexpr span< element_type, dynamic_extent > last( size_type count ) const
      {
         assert( count <= Extent );
         return { data() + Extent - count, count };
      }

      constexpr span< element_type, dynamic_extent > subspan( size_type offset, size_type count = dynamic_extent ) const
      {
         assert( offset <= Extent );
         assert( ( count == dynamic_extent ) || ( count <= ( Extent - offset ) ) );
         return { data() + offset, ( count != dynamic_extent ) ? count : ( Extent - offset ) };
      }

   private:
      pointer m_data;
   };

   template< typename ElementType >
   class span< ElementType, dynamic_extent >
   {
   public:
      static_assert( !std::is_abstract_v< ElementType > );

      using element_type = ElementType;
      using value_type = std::remove_cv_t< ElementType >;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using pointer = element_type*;
      using const_pointer = const element_type*;
      using reference = element_type&;
      using const_reference = const element_type&;
      using iterator = pointer;
      using reverse_iterator = std::reverse_iterator< iterator >;

      static constexpr size_type extent = dynamic_extent;

      constexpr span() noexcept
         : m_data( nullptr ), m_size( 0 )
      {}

      constexpr span( pointer ptr, size_type count ) noexcept
         : m_data( ptr ), m_size( count )
      {}

      constexpr span( pointer first, pointer last ) noexcept
         : m_data( first ), m_size( std::distance( first, last ) )
      {}

      template< std::size_t N >
      constexpr span( element_type ( &arr )[ N ] ) noexcept
         : m_data( arr ), m_size( N )
      {}

      template< std::size_t N, typename = std::enable_if_t< tao::internal::is_span_compatible_ptr< value_type, ElementType >::value > >
      constexpr span( std::array< value_type, N >& arr ) noexcept
         : m_data( arr.data() ), m_size( N )
      {}

      template< std::size_t N, typename = std::enable_if_t< tao::internal::is_span_compatible_ptr< const value_type, ElementType >::value > >
      constexpr span( const std::array< value_type, N >& arr ) noexcept
         : m_data( arr.data() ), m_size( N )
      {}

      template< typename Container, typename = std::enable_if_t< tao::internal::is_span_compatible_container< Container, ElementType >::value > >
      constexpr span( Container& cont )
         : m_data( std::data( cont ) ), m_size( std::size( cont ) )
      {}

      template< typename Container, typename = std::enable_if_t< tao::internal::is_span_compatible_container< const Container, ElementType >::value > >
      constexpr span( const Container& cont )
         : m_data( std::data( cont ) ), m_size( std::size( cont ) )
      {}

      constexpr span( const span& other ) = default;

      template< typename OtherElementType, std::size_t OtherExtent, typename = std::enable_if_t< tao::internal::is_span_compatible_ptr< OtherElementType, ElementType >::value > >
      constexpr span( const span< OtherElementType, OtherExtent >& s ) noexcept
         : m_data( s.data() ), m_size( s.size() )
      {}

      ~span() = default;

      constexpr span& operator=( const span& ) = default;

      constexpr size_type size() const noexcept
      {
         return m_size;
      }

      constexpr size_type size_bytes() const noexcept
      {
         return size() * sizeof( element_type );
      }

      [[nodiscard]] constexpr bool empty() const noexcept
      {
         return size() == 0;
      }

      constexpr reference operator[]( size_type idx ) const noexcept
      {
         assert( idx < size() );
         return *( data() + idx );
      }

      constexpr reference front() const noexcept
      {
         assert( !empty() );
         return *data();
      }

      constexpr reference back() const noexcept
      {
         assert( !empty() );
         return *( data() + ( size() - 1 ) );
      }

      constexpr pointer data() const noexcept
      {
         return m_data;
      }

      constexpr iterator begin() const noexcept
      {
         return data();
      }

      constexpr iterator end() const noexcept
      {
         return data() + size();
      }

      constexpr reverse_iterator rbegin() const noexcept
      {
         return reverse_iterator( end() );
      }

      constexpr reverse_iterator rend() const noexcept
      {
         return reverse_iterator( begin() );
      }

      friend constexpr iterator begin( span s ) noexcept
      {
         return s.begin();
      }

      friend constexpr iterator end( span s ) noexcept
      {
         return s.end();
      }

      template< std::size_t Count >
      constexpr span< element_type, Count > first() const noexcept
      {
         assert( Count <= size() );
         return { data(), Count };
      }

      template< std::size_t Count >
      constexpr span< element_type, Count > last() const noexcept
      {
         assert( Count <= size() );
         return { data() + ( size() - Count ), Count };
      }

      template< std::size_t Offset, std::size_t Count = dynamic_extent >
      constexpr auto subspan() const
         -> span< element_type, ( ( Count != dynamic_extent ) ? Count : dynamic_extent ) >
      {
         assert( Offset <= size() );
         assert( ( Count == dynamic_extent ) || ( Count <= ( size() - Offset ) ) );
         return { data() + Offset, ( Count != dynamic_extent ) ? Count : ( size() - Offset ) };
      }

      constexpr span< element_type, dynamic_extent > first( size_type count ) const
      {
         assert( count <= size() );
         return { data(), count };
      }

      constexpr span< element_type, dynamic_extent > last( size_type count ) const
      {
         assert( count <= size() );
         return { data() + size() - count, count };
      }

      constexpr span< element_type, dynamic_extent > subspan( size_type offset, size_type count = dynamic_extent ) const
      {
         assert( offset <= size() );
         assert( ( count == dynamic_extent ) || ( count <= ( size() - offset ) ) );
         return { data() + offset, ( count != dynamic_extent ) ? count : ( size() - offset ) };
      }

   private:
      pointer m_data;
      size_type m_size;
   };

   template< typename ElementType, std::size_t Extent >
   auto as_bytes( span< ElementType, Extent > s ) noexcept
      -> span< const std::byte, ( ( Extent == dynamic_extent ) ? dynamic_extent : ( sizeof( ElementType ) * Extent ) ) >
   {
      return { reinterpret_cast< const std::byte* >( s.data() ), s.size_bytes() };
   }

   template< typename ElementType, std::size_t Extent, typename = std::enable_if_t< !std::is_const_v< ElementType > > >
   auto as_writable_bytes( span< ElementType, Extent > s ) noexcept
      -> span< std::byte, ( ( Extent == dynamic_extent ) ? dynamic_extent : ( sizeof( ElementType ) * Extent ) ) >
   {
      return { reinterpret_cast< std::byte* >( s.data() ), s.size_bytes() };
   }

   // deduction guides
   template< typename T, std::size_t N >
   span( T ( & )[ N ] )->span< T, N >;

   template< typename T, std::size_t N >
   span( std::array< T, N >& )->span< T, N >;

   template< typename T, std::size_t N >
   span( const std::array< T, N >& )->span< const T, N >;

   template< typename Container >
   span( Container& )->span< typename Container::value_type >;

   template< typename Container >
   span( const Container& )->span< const typename Container::value_type >;

}  // namespace tao

namespace std
{
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

   template< typename ElementType, size_t Extent >
   struct tuple_size< tao::span< ElementType, Extent > >
      : integral_constant< size_t, Extent >
   {};

   template< typename ElementType >
   struct tuple_size< tao::span< ElementType, tao::dynamic_extent > >;  // not defined

   template< size_t I, typename ElementType, size_t Extent >
   struct tuple_element< I, tao::span< ElementType, Extent > >
   {
      static_assert( ( Extent != tao::dynamic_extent ) && ( I < Extent ) );
      using type = ElementType;
   };

#if defined( __clang__ )
#pragma clang diagnostic pop
#endif

   // TODO: this is probably illegal. keep it?
   template< size_t I, typename ElementType, size_t Extent >
   constexpr ElementType& get( tao::span< ElementType, Extent > s ) noexcept
   {
      static_assert( ( Extent != tao::dynamic_extent ) && ( I < Extent ) );
      return s[ I ];
   }

}  // namespace std

#endif

#endif
