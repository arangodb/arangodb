// cxx_function.hpp: major evolution for std::function
// Copyright 2015 by David Krauss.
// This source is released under the MIT license, http://opensource.org/licenses/MIT

#ifndef INCLUDED_CXX_FUNCTION_HPP
#define INCLUDED_CXX_FUNCTION_HPP

#include <cassert>
#include <cstring>
#include <exception>
#include <functional> // for std::bad_function_call and std::mem_fn
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace cxx_function {

#if __cplusplus > 201610
    template< typename t >
    using in_place_t = std::in_place_type_t< t >;

    template< typename t >
    constexpr in_place_t< t > & in_place = std::in_place_type< t >;
#else
    template< typename >
    struct in_place_t {};

#   if __cplusplus >= 201402
    template< typename t >
    constexpr in_place_t< t > in_place = {};
#   endif
#endif

namespace impl {

#define UGLY( NAME ) CXX_FUNCTION_ ## NAME

#if __GNUC__ && ! __clang__ && __GNUC__ < 5
#   define is_trivially_move_constructible has_trivial_copy_constructor
#   define is_trivially_copy_constructible has_trivial_copy_constructor

#   define deprecated(MSG) __attribute__((deprecated (MSG)))

#   define OLD_GCC_FIX(...) __VA_ARGS__
#   define OLD_GCC_SKIP(...)
#else
#   define deprecated(MSG) [[deprecated (MSG)]]

#   define OLD_GCC_FIX(...)
#   define OLD_GCC_SKIP(...) __VA_ARGS__
#endif

#if _MSC_VER
#   define MSVC_FIX(...) __VA_ARGS__
#   define MSVC_SKIP(...)
#else
#   define MSVC_FIX(...)
#   define MSVC_SKIP(...) __VA_ARGS__
#endif

#define UNPACK(...) __VA_ARGS__
#define IGNORE(...)

#define DISPATCH_CQ( MACRO, UNSAFE, TYPE_QUALS, FN_QUALS ) \
    MACRO( TYPE_QUALS, FN_QUALS, UNSAFE ) MACRO( const TYPE_QUALS, const FN_QUALS, IGNORE )
#define DISPATCH_CV( MACRO, UNSAFE, TYPE_QUALS, FN_QUALS ) \
    DISPATCH_CQ( MACRO, UNSAFE, TYPE_QUALS, FN_QUALS ) DISPATCH_CQ( MACRO, IGNORE, volatile TYPE_QUALS, volatile FN_QUALS )

// Apply a given macro over all reference qualifications.
#define DISPATCH_CVREFQ( MACRO, TYPE_QUALS, FN_QUALS ) \
    DISPATCH_CV( MACRO, IGNORE, & TYPE_QUALS, & FN_QUALS ) DISPATCH_CV( MACRO, IGNORE, && TYPE_QUALS, && FN_QUALS )

#define DISPATCH_CVOBJQ( MACRO, UNSAFE, TYPE_QUALS, FN_QUALS ) \
    DISPATCH_CV( MACRO, UNSAFE, & TYPE_QUALS, FN_QUALS ) DISPATCH_CVREFQ( MACRO, TYPE_QUALS, FN_QUALS )

// Apply a given macro over all function qualifications.
#if __cpp_noexcept_function_type
#   define DISPATCH_ALL( MACRO ) DISPATCH_CVOBJQ( MACRO, UNPACK, , ) DISPATCH_CVOBJQ( MACRO, IGNORE, , noexcept )
#else
#   define DISPATCH_ALL( MACRO ) DISPATCH_CVOBJQ( MACRO, UNPACK, , )
#endif

#if __cpp_lib_experimental_logical_traits
    using std::experimental::conjunction;
    using std::experimental::negation;
#else
    template< typename ... cond >
    struct conjunction
        : std::true_type {};
    template< typename next, typename ... cond >
    struct conjunction< next, cond ... >
        : conjunction< typename next::type, cond ... >::type {}; // Not conforming: does not propagate critical type-value.
    template< typename ... cond >
    struct conjunction< std::true_type, cond ... >
        : conjunction< cond ... >::type {};
    template< typename ... cond >
    struct conjunction< std::false_type, cond ... >
        : std::false_type {};
    
    template< typename cond >
    struct negation
        : std::integral_constant< bool, ! cond::value > {};
#endif

// General-purpose dispatch tag.
template< typename ... > struct tag {};

// "Abstract" base class for the island inside the wrapper class, e.g. std::function.
// This must appear first in the wrapper class layout.
struct erasure_base {
    struct erasure_utility const & table;
    
    template< typename derived, typename copyable, typename ... sig >
    erasure_base( tag< derived, copyable, sig ... > ); // Parameters are valueless tags for template deduction.
};

/* Implement a vtable using metaprogramming. Why?
    1. Implement without polymorphic template instantiations (would need 2N of them).
    2. Eliminate overhead and ABI issues associated with RTTI and weak linkage.
    3. Allow static data entries as well as functions.
    
    The table is stored as an aggregate, with a homegrown tuple.
    Entries that would be trivial or useless may be set to nullptr.
    Call dispatch entry types are normalized to non-member, unqualified form.
*/
struct erasure_utility {
    void (* destructor)( erasure_base & );
    void (* move_constructor_destructor)( erasure_base &&, void * dest, void const * source_alloc );
    void (* copy_constructor)( erasure_base const &, void * dest, void const * alloc );
    
    void const * (*target_access)( erasure_base const & );
    std::type_info const & target_type;
    std::type_info const * allocator_type;
};
template< typename ... free >
struct erasure_dispatch {};
template< typename free, typename ... rem >
struct erasure_dispatch< free, rem ... > {
    free * call;
    erasure_dispatch< rem ... > r;
};
template< typename ... free >
struct erasure_table {
    erasure_utility utility; // Initial member of standard-layout struct.
    erasure_dispatch< free ... > dispatch;
};
template< int x, typename free, typename ... rem >
typename std::enable_if< x == 0, free * >::type // Duplicate-type entries occur when overloads differ in qualification.
get( erasure_dispatch< free, rem ... > const & v ) // When this overload is enabled, it is more specialized.
    { return v.call; }
template< int x, typename free, typename ... list >
free * get( erasure_dispatch< list ... > const & v )
    { return get< x - 1, free >( v.r ); }

// Convert a member function signature to its free invocation counterpart.
template< typename sig >
struct member_to_free;

#define TYPE_CONVERT_CASE( TYPE_QUALS, FN_QUALS, UNSAFE ) \
template< typename ret, typename ... arg > \
struct member_to_free< ret( arg ... ) FN_QUALS > \
    { typedef ret type( erasure_base const &, arg && ... ); };
DISPATCH_ALL( TYPE_CONVERT_CASE )
#undef TYPE_CONVERT_CASE
// Allow early, eager conversion without incurring double conversion.
template< typename ret, typename ... arg >
struct member_to_free< ret( erasure_base const &, arg ... ) >
    { typedef ret type( erasure_base const &, arg && ... ); };

// Apply given cv-qualifiers and reference category to a new type.
template< typename source, typename target >
struct transfer_sig_qualifiers;
#define TRANSFER_QUALS_CASE( TYPE_QUALS, FN_QUALS, REFLESS ) \
template< typename ret, typename ... arg, typename target > \
struct transfer_sig_qualifiers< ret( arg ... ) FN_QUALS, target > \
    { typedef target TYPE_QUALS type; };
#define NOEXCEPT
DISPATCH_ALL( TRANSFER_QUALS_CASE )
#undef TRANSFER_QUALS_CASE

// Default, generic "virtual" functions to manage the wrapper payload lifetime.
template< typename derived >
struct erasure_special {
    static void destroy( erasure_base & self ) noexcept
        { static_cast< derived & >( self ). ~ derived(); }
    static void move( erasure_base && self, void * dest, void const * ) {
        new (dest) derived( std::move( static_cast< derived & >( self ) ) );
        destroy( self );
    }
    static void copy( erasure_base const & self, void * dest, void const * )
        { new (dest) derived( static_cast< derived const & >( self ) ); }
};

// These accessors generate "vtable" entries, but avoid instantiating functions that do not exist or would be trivial.
template< typename erasure >
struct erasure_trivially_destructible
    : std::is_trivially_destructible< erasure > {};

template< typename derived >
constexpr typename std::enable_if< ! erasure_trivially_destructible< derived >::value >::type
( * erasure_destroy() ) ( erasure_base & )
    { return & derived::destroy; }
template< typename derived >
constexpr typename std::enable_if< erasure_trivially_destructible< derived >::value >::type
( * erasure_destroy() ) ( erasure_base & )
    { return nullptr; }

template< typename erasure >
struct erasure_trivially_movable : std::integral_constant< bool,
    std::is_trivially_move_constructible< erasure >::value
    && std::is_trivially_destructible< erasure >::value > {};

template< typename derived >
constexpr typename std::enable_if< ! erasure_trivially_movable< derived >::value >::type
( * erasure_move() ) ( erasure_base &&, void *, void const * )
    { return & derived::move; }
template< typename derived >
constexpr typename std::enable_if< erasure_trivially_movable< derived >::value >::type
( * erasure_move() ) ( erasure_base &&, void *, void const * )
    { return nullptr; }

template< typename erasure >
struct erasure_nontrivially_copyable : std::integral_constant< bool,
    std::is_copy_constructible< erasure >::value
    && ! std::is_trivially_copy_constructible< erasure >::value > {};

template< typename derived, typename enable >
constexpr typename std::enable_if< std::enable_if< enable::value, erasure_nontrivially_copyable< derived > >::type::value >::type
( * erasure_copy( enable * ) ) ( erasure_base const &, void *, void const * )
    { return & derived::copy; }
template< typename derived >
constexpr void ( * erasure_copy( ... ) ) ( erasure_base const &, void *, void const * )
    { return nullptr; }


// Collect the above into global tables.
// typename copyable enables non-trivial copyability. Trivially copyable and noncopyable tables both set it to false.
// For trivial, pointer-like target types, sig is actually the sequence of free function types.
#define VTABLE_INITIALIZER = { \
    erasure_destroy< erasure >(), \
    erasure_move< erasure >(), \
    erasure_copy< erasure >( static_cast< copyable * >( nullptr ) ), \
    & erasure::target_access, \
    typeid (typename erasure::target_type), \
    erasure::common_allocator_type_info, \
    & erasure::template call< sig > ..., \
    erasure_dispatch<>{} \
}
#ifdef __GNUC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wmissing-braces" // Need brace elision here.
#endif
template< typename erasure, typename copyable, typename ... sig >
struct make_erasure_table {
    static MSVC_SKIP (constexpr) erasure_table< typename member_to_free< sig >::type ... > const value MSVC_SKIP( VTABLE_INITIALIZER );
};
#ifdef __GNUC__
#   pragma GCC diagnostic pop
#endif
template< typename erasure, typename copyable, typename ... sig >
MSVC_SKIP (constexpr) erasure_table< typename member_to_free< sig >::type ... > const
make_erasure_table< erasure, copyable, sig ... >::value MSVC_FIX( VTABLE_INITIALIZER );

template< typename derived, typename copyable, typename ... sig >
erasure_base::erasure_base( tag< derived, copyable, sig ... > )
    : table( make_erasure_table< derived, copyable, sig ... >::value.utility ) {}


// Implement the uninitialized state.
struct null_erasure
    : erasure_base // "vtable" interface class
    , erasure_special< null_erasure > { // generic implementations of "virtual" functions
    typedef void target_type;
    static constexpr std::type_info const * common_allocator_type_info = nullptr;
    
    // The const qualifier is bogus. Rather than type-erase an identical non-const version, let the wrapper do a const_cast.
    static void const * target_access( erasure_base const & ) { return nullptr; } // target<void>() still returns nullptr.
    
    template< typename ... sig >
    null_erasure( tag< sig ... > ) noexcept
        : erasure_base( tag< null_erasure, std::false_type, sig ... >{} ) {} // Initialize own "vtable pointer" at runtime.
    
    template< typename, typename ret, typename ... arg >
    static ret call( erasure_base const &, arg ... )
        { throw std::bad_function_call{}; }
};

// Implement erasures of function pointers and std::reference_wrapper, which avoid any allocation.
// For now, small user-defined target types piggyback here. The only concession is accepting a "copyable" flag.
template< typename in_target_type >
struct local_erasure
    : erasure_base
    , erasure_special< local_erasure< in_target_type > > {
    typedef in_target_type target_type;
    target_type target;
    static constexpr std::type_info const * common_allocator_type_info = nullptr;
    
    static void const * target_access( erasure_base const & self )
        { return & static_cast< local_erasure const & >( self ).target; }
    
    template< typename copyable, typename ... free, typename ... arg >
    local_erasure( tag< copyable, free ... >, arg && ... a )
        : erasure_base( tag< local_erasure, copyable, free ... >{} )
        , target( std::forward< arg >( a ) ... ) {}
    
    template< typename sig, typename ret, typename ... arg >
    static ret call( erasure_base const & self, arg ... a )
        // Directly call the name "target," not a reference, to support devirtualization.
        { return ( (typename transfer_sig_qualifiers< sig, local_erasure >::type) self ).target( std::forward< arg >( a ) ... ); }
};

// Implement erasures of pointer-to-members, which need std::mem_fn instead of a direct call.
template< typename in_target_type >
struct ptm_erasure
    : erasure_base
    , erasure_special< ptm_erasure< in_target_type > > {
    typedef in_target_type target_type;
    target_type target; // Do not use mem_fn here...
    static constexpr std::type_info const * common_allocator_type_info = nullptr;
    
    static void const * target_access( erasure_base const & self )
        { return & static_cast< ptm_erasure const & >( self ).target; } // ... because the user can get read/write access to the target object.
    
    template< typename ... free >
    ptm_erasure( tag< free ... >, target_type a )
        : erasure_base( tag< ptm_erasure, std::false_type, free ... >{} )
        , target( a ) {}
    
    template< typename, typename ret, typename ... arg >
    static ret call( erasure_base const & self, arg ... a )
        { return ( std::mem_fn( static_cast< ptm_erasure const & >( self ).target ) )( std::forward< arg >( a ) ... ); }
};

// Implement erasures of objects that cannot be stored inside the wrapper.
/*  This does still store the allocator and pointer in the wrapper. A more general case should be added.
    However, there is a conundrum in rebinding an allocator to an instance of itself.
    Also, it's not clear that a native pointer will always be stable, as opposed to a fancy pointer.
    Fancy pointers exceeding the wrapper storage, with varying underlying referent storage, are another conundrum. */
// Use Allocator<char> as a common reference point, for the typeid operator and the instance in function_container.
// (The instance in the erasure object is always a bona fide Allocator<T>, though.)
template< typename allocator >
using common_allocator_rebind = typename std::allocator_traits< allocator >::template rebind_alloc< char >;

template< typename t, typename = void >
struct not_always_equal_by_expression : std::true_type {};
template< typename t >
using id_t = t; // MSVC can't parse or SFINAE on a nested-name-specifier with decltype.
template< typename t >
struct not_always_equal_by_expression< t, typename std::enable_if< id_t< decltype (std::declval< t >() == std::declval< t >()) >::value >::type >
    : std::false_type {};
template< typename t, typename = void >
struct not_always_equal_by_trait : std::true_type {};
template< typename t >
struct not_always_equal_by_trait< t, typename std::enable_if< std::allocator_traits< t >::is_always_equal::value >::type >
    : std::false_type {};
template< typename t >
struct is_always_equal : negation< conjunction< not_always_equal_by_trait< t >, not_always_equal_by_expression< t > > > {};
template< typename t >
struct is_always_equal< std::allocator< t > > : std::true_type {};

template< typename allocator_in, typename in_target_type >
struct allocator_erasure
    : erasure_base
    , allocator_in { // empty base class optimization (EBCO)
    typedef std::allocator_traits< allocator_in > allocator_traits;
    typedef common_allocator_rebind< allocator_in > common_allocator;
    static MSVC_SKIP (constexpr) std::type_info const * const common_allocator_type_info
        MSVC_SKIP ( = & typeid (allocator_erasure::common_allocator) );
    
    typedef in_target_type target_type;
    typename allocator_traits::pointer target;
    
    allocator_in & alloc() { return static_cast< allocator_in & >( * this ); }
    allocator_in const & alloc() const { return static_cast< allocator_in const & >( * this ); }
    target_type * target_address() { return std::addressof( * target ); }
    static void const * target_access( erasure_base const & self )
        { return std::addressof( * static_cast< allocator_erasure const & >( self ).target ); }
    
    template< typename ... arg >
    void construct_safely( arg && ... a ) try {
        allocator_traits::construct( alloc(), target_address(), std::forward< arg >( a ) ... );
    } catch (...) {
        allocator_traits::deallocate( alloc(), target, 1 ); // Does not throw according to [allocator.requirements] §17.6.3.5 and DR2384.
        throw;
    } // The wrapper allocator instance cannot be updated following a failed initialization because the erasure allocator is already gone.
    
    allocator_erasure( allocator_erasure && ) = default; // Called by move( true_type{}, ... ) when allocator or pointer is nontrivially movable.
    allocator_erasure( allocator_erasure const & ) = delete;
    
    template< typename copyable, typename ... sig, typename ... arg >
    allocator_erasure( tag< copyable, sig ... >, std::allocator_arg_t, allocator_in const & in_alloc, arg && ... a )
        : erasure_base( tag< allocator_erasure, copyable, sig ... >{} )
        , allocator_in( in_alloc )
        , target( allocator_traits::allocate( alloc(), 1 ) )
        { construct_safely( std::forward< arg >( a ) ... ); }
private:
    // Copy or move. Moving only occurs to a different pool.
    template< typename source >
    allocator_erasure( std::allocator_arg_t, allocator_in const & dest_allocator, source && o )
        : erasure_base( o )
        , allocator_in( dest_allocator )
        , target( allocator_traits::allocate( alloc(), 1 ) ) {
        construct_safely( std::forward< typename std::conditional< std::is_lvalue_reference< source >::value,
                                                                    target_type const &, target_type && >::type >( * o.target ) );
    }
public:
    void move( std::true_type, void * dest, void const * ) noexcept { // Call ordinary move constructor.
        new (dest) allocator_erasure( std::move( * this ) ); // Move the pointer, not the object. Don't call the allocator at all.
        this-> ~ allocator_erasure();
    }
    void move( std::false_type, void * dest, void const * dest_allocator_v ) {
        auto * dest_allocator_p = static_cast< common_allocator const * >( dest_allocator_v ); // The wrapper verified the safety of this using typeid.
        if ( ! dest_allocator_p || * dest_allocator_p == alloc() ) {
            move( std::true_type{}, dest, dest_allocator_v ); // same pool
        } else { // different pool
            new (dest) allocator_erasure( std::allocator_arg, static_cast< allocator_in >( * dest_allocator_p ), // Reallocate.
                std::move_if_noexcept( * this ) ); // Protect user against their own throwing move constructors.
            destroy( * this );
        }
    }
    // [*_]allocator_v points to the wrapper allocator instance, if any.
    static void move( erasure_base && self_base, void * dest, void const * dest_allocator_v ) {
        auto & self = static_cast< allocator_erasure & >( self_base );
        // is_always_equal is usually false here, because it correlates with triviality which short-circuits this function.
        std::move( self ).move( MSVC_FIX (impl::) is_always_equal< allocator_in >{}, dest, dest_allocator_v );
    }
    static void copy( erasure_base const & self_base, void * dest, void const * dest_allocator_v ) {
        auto & self = static_cast< allocator_erasure const & >( self_base );
        // Structure the control flow differently to avoid instantiating the copy constructor.
        allocator_in const & dest_allocator = dest_allocator_v?
            static_cast< allocator_in const & >( * static_cast< common_allocator const * >( dest_allocator_v ) )
            : self.alloc();
        new (dest) allocator_erasure( std::allocator_arg, dest_allocator, self );
    }
    static void destroy( erasure_base & self_base ) noexcept {
        auto & self = static_cast< allocator_erasure & >( self_base );
        allocator_traits::destroy( self.alloc(), self.target_address() );
        allocator_traits::deallocate( self.alloc(), self.target, 1 );
        self. ~ allocator_erasure();
    }
    
    template< typename sig, typename ret, typename ... arg >
    static ret call( erasure_base const & self, arg ... a ) {
        return std::forward< typename transfer_sig_qualifiers< sig, target_type >::type >
            ( * static_cast< allocator_erasure const & >( self ).target )( std::forward< arg >( a ) ... );
    }
};
template< typename allocator_in, typename in_target_type >
MSVC_SKIP (constexpr) std::type_info const * const allocator_erasure< allocator_in, in_target_type >::common_allocator_type_info
    MSVC_FIX ( = & typeid (allocator_erasure::common_allocator) );

template< typename allocator, typename target_type >
struct erasure_trivially_destructible< allocator_erasure< allocator, target_type > >
    : std::false_type {};

template< typename allocator, typename target_type >
struct erasure_trivially_movable< allocator_erasure< allocator, target_type > > : std::integral_constant< bool,
    std::is_trivially_move_constructible< allocator_erasure< allocator, target_type > >::value
    && std::is_trivially_destructible< allocator_erasure< allocator, target_type > >::value
    && is_always_equal< allocator >::value > {};

template< typename allocator, typename target_type >
struct erasure_nontrivially_copyable< allocator_erasure< allocator, target_type > >
    : std::is_copy_constructible< target_type > {};


// Forbid certain return value conversions that require temporaries.
template< typename from, typename to > // Primary case: from is object type. A prvalue is returned. Only allow user-defined conversions.
struct is_bad_return // Converting a prvalue to a reference binds a temporary unless the prvalue is a class and the reference is not a base.
    : negation< conjunction< std::is_class< from >, negation< std::is_base_of< to, from > > > > {};

template< typename from, typename to > // Treat xvalues like lvalues.
struct is_bad_return< from &&, to > : is_bad_return< from &, to > {};

template< typename from, typename to > // From is reference type. Allow user-defined and derived-to-base conversions.
struct is_bad_return< from &, to > // Forbid converting an lvalue to a reference unless it's a class or the types match.
    : conjunction< negation< std::is_same< typename std::remove_cv< from >::type, to > >, negation< std::is_class< from > > > {};

template< typename from, typename to >
struct is_safely_convertible
    : conjunction<
        negation< conjunction< // Prevent binding a return value to a temporary.
            std::is_reference< to >,
            is_bad_return< from, typename std::decay< to >::type >
        > >,
        std::is_convertible< from, to >
    >::type {};

template< typename t, typename sig, typename = void >
struct is_callable : std::false_type {};

#define IS_CALLABLE_CASE( TYPE_QUALS, FN_QUALS, UNSAFE ) \
template< typename t, typename ret, typename ... arg > \
struct is_callable< t, ret( arg ... ) FN_QUALS, \
    typename std::enable_if< is_safely_convertible< \
        typename std::result_of< t TYPE_QUALS ( arg ... ) >::type \
    , ret >::value >::type > \
    : std::true_type {};
DISPATCH_CVOBJQ( IS_CALLABLE_CASE, IGNORE, , )
#undef IS_CALLABLE_CASE

template< typename sig >
struct is_callable< std::nullptr_t, sig >
    : std::true_type {};

#if __cpp_noexcept_function_type

#   if __GNUC__ && ! __clang__ && __GNUC__ < 7
#       define IS_NOTHROW_INVOKABLE is_nothrow_callable
#   else
#       define IS_NOTHROW_INVOKABLE is_nothrow_invocable
#   endif

#   define NOEXCEPT_CASE( TYPE_QUALS, FN_QUALS, UNSAFE ) \
    template< typename t, typename ret, typename ... arg > \
    struct is_callable< t, ret( arg ... ) FN_QUALS noexcept, \
        typename std::enable_if< std::IS_NOTHROW_INVOKABLE< t TYPE_QUALS ( arg ... ), ret >::value >::type > \
        : is_callable< t, ret( arg ... ) FN_QUALS > {};
    DISPATCH_CVOBJQ( NOEXCEPT_CASE, IGNORE, , )
#   undef NOEXCEPT_CASE
#   undef IS_NOTHROW_INVOKABLE

#   define NOEXCEPT_NULLPTR_CASE( TYPE_QUALS, FN_QUALS, UNSAFE ) \
    template< typename ret, typename ... arg > \
    struct is_callable< std::nullptr_t, ret( arg ... ) FN_QUALS > : std::false_type {};
    DISPATCH_CVOBJQ( NOEXCEPT_NULLPTR_CASE, IGNORE, , noexcept )
#   undef NOEXCEPT_NULLPTR_CASE
#endif

template< typename ... sig >
struct is_all_callable {
    template< typename t >
    using temp = conjunction< is_callable< t, sig > ... >;
    
    typedef std::false_type copies;
};

template< typename self, typename ... sig >
struct is_copyable_all_callable {
    template< typename t, typename = void >
    struct temp : conjunction< std::is_copy_constructible< t >, typename is_all_callable< sig ... >::template temp< t > >::type {};
    
    template< typename v > // Presume that self is a copyable wrapper, since that is what uses this metafunction.
    struct temp< self, v > : std::true_type {}; // Avoid inspecting incomplete self.
    
    typedef std::true_type copies;
};

// Map privileged types to noexcept specifications.
template< typename source >
struct is_noexcept_erasable : std::false_type {};
template<>
struct is_noexcept_erasable< std::nullptr_t > : std::true_type {};
template< typename t >
struct is_noexcept_erasable< t * > : std::true_type {};
template< typename t, typename c >
struct is_noexcept_erasable< t c::* > : std::true_type {};
template< typename t >
struct is_noexcept_erasable< std::reference_wrapper< t > > : std::true_type {};

// Termination of the recursive template generated by WRAPPER_CASE.
template< typename derived, std::size_t n, typename ... sig >
struct wrapper_dispatch {
    static_assert ( sizeof ... (sig) == 0, "An unsupported function signature was detected." );
    void operator () ( wrapper_dispatch ) = delete; // Feed the "using operator ();" or "using call;" declaration in next derived class.
};

template< typename >
struct const_unsafe_case; // internal tag for function signatures introduced for backward compatibility of const-qualified access

// Generate the wrapper dispatcher by brute-force specialization over qualified function types.

// This macro generates a recursive template handling one type qualifier sequence, e.g. "volatile &" or "const."
// The final product converts a sequence of qualified signatures into an overload set, potentially with special cases for signatures of no qualification.
#define WRAPPER_CASE( \
    TYPE_QUALS, FN_QUALS, /* The type qualifiers for this case. */ \
    UNSAFE /* UNPACK if there are no qualifiers, IGNORE otherwise. Supports deprecated const-qualified access. */ \
) \
template< typename derived, std::size_t table_index, typename ret, typename ... arg, typename ... sig > \
struct wrapper_dispatch< derived, table_index, ret( arg ... ) FN_QUALS, sig ... > \
    : wrapper_dispatch< derived, table_index+1, sig ... \
        UNSAFE (, const_unsafe_case< ret( arg ... ) >) > { \
    using wrapper_dispatch< derived, table_index+1, sig ... \
        UNSAFE (, const_unsafe_case< ret( arg ... ) >) >::operator (); \
    ret operator () ( arg ... a ) FN_QUALS \
        { return ( (derived const &) * this ).template call< table_index, ret >( std::forward< arg >( a ) ... ); } \
};
DISPATCH_ALL( WRAPPER_CASE )
#undef WRAPPER_CASE

// Additionally implement the legacy casting away of const, but with a warning.
template< typename derived, std::size_t n, typename ret, typename ... arg, typename ... more >
struct wrapper_dispatch< derived, n, const_unsafe_case< ret( arg ... ) >, more ... >
    : wrapper_dispatch< derived, n, more ... > {
    using wrapper_dispatch< derived, n, more ... >::operator ();
    deprecated( "It is unsafe to call a std::function of non-const signature through a const access path." )
    ret operator () ( arg ... a ) const
        { return ( (derived &) * this ) ( std::forward< arg >( a ) ... ); }
};

struct allocator_mismatch_error : std::exception // This should be implemented in a .cpp file, but stay header-only for now.
    { virtual char const * what() const noexcept override { return "An object could not be transferred into an incompatible memory allocation scheme."; } };

template< typename erasure_base, typename allocator >
typename std::enable_if< ! std::is_void< allocator >::value,
bool >::type nontrivial_target( erasure_base const * e, allocator const * ) {
    std::type_info const * type = e->table.allocator_type;
    if ( type == nullptr ) return false; // It's a raw pointer, PTMF, reference_wrapper, or nullptr.
    if ( * type != typeid (allocator) ) throw allocator_mismatch_error{}; // Target belongs to a different allocation scheme.
    return true;
}
inline bool nontrivial_target( void const *, void const * ) { return true; } // Give false positives when not checking the allocator.

template< typename ... free >
class wrapper_base {
    template< typename, typename ... >
    friend class wrapper;
    typedef std::aligned_storage< sizeof (void *[3]) >::type effective_storage_type;
protected:
    std::aligned_storage< sizeof (void *[4]), alignof(effective_storage_type) >::type storage;
    void * storage_address() { return & storage; }
    
    // init and destroy enter or recover from invalid states.
    // They get on the right side of [basic.life]/7.4, but mind the exceptions.
    
    // Default, move, and copy construction.
    // Adopt by move.
    template< typename allocator = void > // Allocator is already rebound to <char>.
    void init( wrapper_base && s, allocator const * dest_alloc = nullptr ) {
        decltype (erasure_utility::move_constructor_destructor) nontrivial;
        if ( ! nontrivial_target( & s.erasure(), dest_alloc ) // No-op without an allocator. Save an access in pointer-like target case otherwise.
            || ! ( nontrivial = s.erasure().table.move_constructor_destructor ) ) {
            std::memcpy( & this->erasure(), & s.erasure(), sizeof (storage) );
        } else nontrivial( std::move( s.erasure() ), & this->erasure(), dest_alloc );
        s.emplace_trivial( in_place_t< std::nullptr_t >{} );
    }
    
    // Adopt by copy.
    template< typename allocator = void >
    void init( wrapper_base const & s, allocator const * dest_alloc = nullptr ) {
        decltype (erasure_utility::copy_constructor) nontrivial;
        if ( ! nontrivial_target( & s.erasure(), dest_alloc ) // No-op without an allocator. Save an access in pointer-like target case otherwise.
            || ! ( nontrivial = s.erasure().table.copy_constructor ) ) {
            std::memcpy( & this->erasure(), & s.erasure(), sizeof (storage) );
        } else nontrivial( s.erasure(), & this->erasure(), dest_alloc );
    }
    
    template< typename source > // Target value is nullptr or a default-constructed pointer, which compares equal to nullptr.
    void emplace_trivial( in_place_t< source >, source = {} ) noexcept
        { new (storage_address()) null_erasure( tag< free ... >{} ); }
    
    // Pointers, PTMs, and reference wrappers bypass allocators completely.
    template< typename t >
    void emplace_trivial( in_place_t< t * >, t * p ) noexcept {
        if ( p ) new (storage_address()) local_erasure< t * >( tag< std::false_type, free ... >{}, p );
        else emplace_trivial( in_place_t< std::nullptr_t >{} );
    }
    template< typename t, typename c >
    void emplace_trivial( in_place_t< t c::* >, t c::* ptm ) noexcept {
        if ( ptm ) new (storage_address()) ptm_erasure< t c::* >( tag< free ... >{}, ptm );
        else emplace_trivial( in_place_t< std::nullptr_t >{} );
    }
    template< typename t >
    void emplace_trivial( in_place_t< std::reference_wrapper< t > >, std::reference_wrapper< t > r ) noexcept {
        new (storage_address()) local_erasure< std::reference_wrapper< t > >( tag< std::false_type, free ... >{}, r );
    }
    
    void destroy() noexcept {
        auto nontrivial = erasure().table.destructor;
        if ( nontrivial ) nontrivial( erasure() );
    }
    
    // Implement erasure type verification for always-local targets without touching RTTI.
    bool verify_type_impl( void * ) const noexcept
        { return & erasure().table == & make_erasure_table< null_erasure, std::false_type, free ... >::value.utility; }
    
    template< typename t >
    bool verify_type_impl( std::reference_wrapper< t > * ) const noexcept {
        return & erasure().table
            == & make_erasure_table< local_erasure< std::reference_wrapper< t > >, std::false_type, free ... >::value.utility;
    }
    template< typename t >
    bool verify_type_impl( t ** ) const noexcept
        { return & erasure().table == & make_erasure_table< local_erasure< t * >, std::false_type, free ... >::value.utility; }
    
    template< typename t, typename c >
    bool verify_type_impl( t c::** ) const noexcept
        { return & erasure().table == & make_erasure_table< ptm_erasure< t c::* >, std::false_type, free ... >::value.utility; }
    
    // User-defined class types are never guaranteed to be local. There could exist some allocator for which uses_allocator is true.
    // RTTI could be replaced here by a small variable template linked from the table. Since we need it anyway, just use RTTI.
    template< typename want >
    bool verify_type_impl( want * ) const noexcept
        { return target_type() == typeid (want); }
public:
    erasure_base & erasure()
        { return reinterpret_cast< erasure_base & >( storage ); }
    erasure_base const & erasure() const
        { return reinterpret_cast< erasure_base const & >( storage ); }
    
    void operator = ( wrapper_base && s ) noexcept { // Only suitable for generating implicit members in derived classes: wrong return type.
        destroy();
        init( std::move( s ) );
    }
    void operator = ( wrapper_base const & s ) {
        wrapper_base temp;
        temp.init( s );
        destroy();
        init( std::move( temp ) );
    }
    template< typename source >
    typename std::enable_if< is_noexcept_erasable< source >::value >::type
    operator = ( source const & s ) noexcept {
        destroy();
        emplace_trivial( in_place_t< source >{}, s );
    }
    
    template< std::size_t table_index, typename ret, typename ... arg >
    ret call( arg && ... a ) const {
        return get< table_index, ret( erasure_base const &, arg && ... ) >
            ( reinterpret_cast< erasure_table< free ... > const & >( erasure().table ).dispatch )
            ( erasure(), std::forward< arg >( a ) ... );
    }
    
    std::type_info const & target_type() const noexcept
        { return erasure().table.target_type; }
    
    template< typename want >
    bool verify_type() const noexcept {
        static_assert ( std::is_same< want, typename std::decay< want >::type >::value, "function target can only be of cv-unqualified object type." );
        return verify_type_impl( (want *) nullptr );
    }
    template< typename want >
    bool verify_type() const volatile noexcept
        { return const_cast< wrapper_base const * >( this )->verify_type< want >(); }
    
    void const * complete_object_address() const noexcept
        { return erasure().table.target_access( erasure() ); }
    void const volatile * complete_object_address() const volatile noexcept
        { return const_cast< wrapper_base const * >( this )->complete_object_address(); }
    
    template< typename want >
    want const * target() const noexcept {
        if ( ! verify_type< want >() ) return nullptr;
        return static_cast< want const * >( erasure().table.target_access( erasure() ) );
    }
    template< typename want >
    want * target() noexcept
        { return const_cast< want * >( static_cast< wrapper_base const & >( * this ).target< want >() ); }
    
    explicit operator bool () const volatile noexcept
        { return ! verify_type< void >(); }
};

template< typename t >
typename std::remove_cv< t >::type value( t & v ) { return v; }
template< typename t >
t value( t const && v ) { return std::move( v ); }
template< typename t >
t && value( t && v ) { return std::move( v ); }

template< typename target, typename ... free >
typename std::enable_if< conjunction<
    std::is_same< target, typename target::UGLY(wrapper_type) >
    , std::is_base_of< wrapper_base< free ... >, target > >::value,
bool >::type is_empty_wrapper( target *, wrapper_base< free ... > const * arg )
    { return ! * arg; }
inline bool is_empty_wrapper( ... ) { return false; }

template< bool is_compatibly_wrapped, typename target, typename allocator >
struct rebind_allocator_for_source
    { typedef typename std::allocator_traits< allocator >::template rebind_alloc< target > type; };
template< typename target, typename allocator >
struct rebind_allocator_for_source< true, target, allocator >
    { typedef common_allocator_rebind< allocator > type; };

template< typename derived, bool >
struct nullptr_construction {
    nullptr_construction() noexcept
        { static_cast< derived * >( this )->emplace_trivial( in_place_t< std::nullptr_t >{} ); }
    nullptr_construction( tag< struct trivialize > ) {}
};
template< typename derived >
struct nullptr_construction< derived, false > {
    nullptr_construction() noexcept = delete;
    nullptr_construction( tag< struct trivialize > ) {}
};

template< typename target_policy, typename ... sig >
class wrapper
    : public wrapper_base< typename member_to_free< sig >::type ... >
    , private nullptr_construction< wrapper< target_policy, sig ... >, target_policy::template temp< std::nullptr_t >::value >
    , public wrapper_dispatch< wrapper< target_policy, sig ... >, 0, sig ... > {
    using wrapper::wrapper_base::storage;
    using wrapper::wrapper_base::storage_address;
    using wrapper::wrapper_base::init;
    using wrapper::wrapper_base::emplace_trivial;
    
protected: // Queries on potential targets. Shared with container_wrapper.
    template< typename target >
    using is_targetable = typename target_policy::template temp< target >;
    
    template< typename source >
    struct is_small {
        static const bool value = sizeof (local_erasure< source >) <= sizeof (storage)
            && alignof (decltype (storage)) % alignof (source) == 0
            && std::is_nothrow_move_constructible< source >::value;
    };
    
    template< typename source, typename = source, typename = typename wrapper::wrapper_base >
    struct is_compatibly_wrapped : std::false_type {};
    template< typename source >
    struct is_compatibly_wrapped< source, typename source::UGLY(wrapper_type), typename source::wrapper_base >
        : std::true_type {};
    
    template< typename source, typename allocator >
    bool nontrivial_target( source const & s, allocator const * alloc )
        { return impl::nontrivial_target( & s.erasure(), alloc ); } // Use friend relationship with compatible wrappers.
private:
    template< typename target, typename allocator, typename ... arg >
    void allocate( allocator * alloc, arg && ... a ) {
        if ( is_empty_wrapper( (target *) nullptr, & a ... ) ) {
            return emplace_trivial( in_place_t< std::nullptr_t >{} );
        }
        typedef allocator_erasure< allocator, target > erasure;
        // TODO: Add a new erasure template to put the fancy pointer on the heap.
        static_assert ( sizeof (erasure) <= sizeof storage, "Stateful allocator or fancy pointer is too big for polymorphic function wrapper." );
        new (storage_address()) erasure( tag< typename target_policy::copies, sig ... >{}, std::allocator_arg, * alloc, std::forward< arg >( a ) ... );
    }
    
    // When value-wise initialization isn't adoption, delegate to in-place construction or allocation.
    template< typename source >
    typename std::enable_if< ! is_compatibly_wrapped< typename std::decay< source >::type >::value >::type
    init( source && s )
        { emplace< typename std::decay< source >::type >( std::forward< source >( s ) ); }
    template< typename source, typename allocator >
    typename std::enable_if< ! is_compatibly_wrapped< typename std::decay< source >::type >::value >::type
    init( source && s, allocator * a )
        { allocate< typename std::decay< source >::type >( a, std::forward< source >( s ) ); }
    
    template< typename source, typename ... arg >
    typename std::enable_if< is_small< source >::value && ! is_noexcept_erasable< source >::value >::type
    emplace( arg && ... a )
        { new (storage_address()) local_erasure< source >( tag< typename target_policy::copies, sig ... >{}, std::forward< arg >( a ) ... ); }
    
    template< typename source, typename ... arg >
    typename std::enable_if< is_noexcept_erasable< source >::value >::type
    emplace( arg && ... a )
        { emplace_trivial( in_place_t< source >{}, std::forward< arg >( a ) ... ); }
    template< typename target, typename ... arg >
    typename std::enable_if< ! is_small< target >::value >::type
    emplace( arg && ... a ) {
        std::allocator< target > alloc;
        allocate< target >( & alloc, std::forward< arg >( a ) ... );
    }
    
public:
    typedef wrapper UGLY(wrapper_type);
    
    wrapper() noexcept = default;
    
    friend typename wrapper::nullptr_construction;
    #define NON_NULLPTR_CONSTRUCT : wrapper::nullptr_construction( tag< trivialize >{} )
    
    wrapper( wrapper && s ) noexcept NON_NULLPTR_CONSTRUCT
        { init( std::move( s ) ); }
    wrapper( wrapper const & s ) NON_NULLPTR_CONSTRUCT
        { init( s ); }
    
MSVC_SKIP ( // MSVC cannot parse deprecated constructor templates, so remove this entirely.
    template< typename allocator >
    deprecated( "This constructor ignores its allocator argument. Specify allocation per-target or use function_container instead." )
    wrapper( std::allocator_arg_t, allocator const & ) NON_NULLPTR_CONSTRUCT
        { init( in_place_t< std::nullptr_t >{}, nullptr ); }
)
    template< typename source,
        typename std::enable_if< conjunction<
            negation< std::is_base_of< wrapper, typename std::decay< source >::type > >
            , is_targetable< typename std::decay< source >::type >
        >::value >::type * = nullptr >
    wrapper( source && s )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value
            || is_compatibly_wrapped< source >::value ) NON_NULLPTR_CONSTRUCT
        { init( std::forward< source >( s ) ); }
    
    template< typename allocator, typename source,
        typename = typename std::enable_if<
            is_targetable< typename std::decay< source >::type >::value
        >::type >
    wrapper( std::allocator_arg_t, allocator && alloc, source && s )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value ) NON_NULLPTR_CONSTRUCT {
        // Adapt the allocator to the source. If it's already an rvalue of the right type, use it in-place.
        // It might make more sense to use modifiable lvalues in-place, but this is closer to the standard.
        typename rebind_allocator_for_source< is_compatibly_wrapped< typename std::decay< source >::type >::value,
            typename std::decay< source >::type, typename std::decay< allocator >::type
        >::type && target_alloc = value( std::forward< allocator >( alloc ) );
        init( std::forward< source >( s ), & target_alloc );
    }
    
    template< typename target, typename ... arg,
        typename = typename std::enable_if< is_targetable< target >::value >::type >
    wrapper( in_place_t< target >, arg && ... a )
    noexcept( is_noexcept_erasable< target >::value )
        { emplace< target >( std::forward< arg >( a ) ... ); }
    
    template< typename allocator, typename target, typename ... arg,
        typename = typename std::enable_if<
            is_targetable< target >::value
        >::type >
    wrapper( std::allocator_arg_t, allocator && alloc, in_place_t< target >, arg && ... a )
    noexcept( is_noexcept_erasable< target >::value ) {
        typename std::allocator_traits< typename std::decay< allocator >::type >::template rebind_alloc< target >
            && target_alloc = value( std::forward< allocator >( alloc ) );
        allocate< target >( & target_alloc, std::forward< arg >( a ) ... );
    }
    #undef NON_NULLPTR_CONSTRUCT
    
    wrapper & operator = ( wrapper && ) = default;
    wrapper & operator = ( wrapper const & ) = default;
    template< typename source >
    typename std::enable_if<
        is_noexcept_erasable< typename std::decay< source >::type >::value
        || is_compatibly_wrapped< typename std::decay< source >::type >::value
    >::type operator = ( source && s )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value || is_compatibly_wrapped< source >::value )
        { wrapper::wrapper_base::operator = ( std::forward< source >( s ) ); }
    
    template< typename source >
    typename std::enable_if<
        ! is_noexcept_erasable< typename std::decay< source >::type >::value
        && ! is_compatibly_wrapped< typename std::decay< source >::type >::value
    >::type operator = ( source && s )
        { wrapper::wrapper_base::operator = ( wrapper( std::forward< source >( s ) ) ); }
    
    template< typename source, typename allocator >
    void assign( source && s, allocator const & alloc )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value )
        { * this = wrapper( std::allocator_arg, alloc, std::forward< source >( s ) ); }
    
    // Quirk: If emplacing constructor throws, but move constructor does not, and the target is small, it may be moved for the sake of strong exception safety.
    template< typename source, typename ... arg >
    void emplace_assign( arg && ... a ) // TODO separate non-container case avoiding saved_allocator.
    noexcept( is_noexcept_erasable< source >::value )
        { * this = wrapper( in_place_t< source >{}, std::forward< arg >( a ) ... ); }
    
    template< typename source, typename allocator, typename ... arg >
    void allocate_assign( allocator const & alloc, arg && ... a )
    noexcept( is_noexcept_erasable< source >::value )
        { * this = wrapper( std::allocator_arg, alloc, in_place_t< source >{}, std::forward< arg >( a ) ... ); }
    
    ~ wrapper() noexcept
        { this->destroy(); }
    
    void swap( wrapper & o ) noexcept { // Essentially same as std::swap, but skip a couple trivial destructors.
        // Would be nice to use swap_ranges on trivially-copyable target storage, but that wouldn't correctly handle lifetimes in the upcoming rules.
        wrapper temp{ std::move( * this ) };
        init( in_place_t< wrapper >{}, std::move( o ) );
        o.init( in_place_t< wrapper >{}, std::move( temp ) );
    }
};

template< typename saved_allocator, typename used_allocator >
void assign_allocator( std::true_type, saved_allocator * d, used_allocator * s )
    { * d = std::move( * s ); }
inline void assign_allocator( std::false_type, void *, void const * ) {}

template< typename allocator >
void allocator_swap( std::true_type, allocator * lhs, allocator * rhs )
    { std::iter_swap( lhs, rhs ); }
template< typename allocator >
void allocator_swap( std::false_type, allocator * lhs, allocator * rhs )
    { assert ( * lhs == * rhs && "Cannot swap containers with non-swapping, unequal allocators." ); }

template< typename source > // If the propagation trait is true_type, and the source is a compatible container, then get its allocator.
typename source::UGLY(common_allocator) && select_allocator( std::true_type, void *, source const * s )
    { return std::move( s->saved_allocator() ); }
template< typename allocator >
allocator && select_allocator( std::false_type, allocator * a, void const * )
    { return std::move( * a ); }

/*  One or two Allocator objects are stored in a container_wrapper. The allocator base class below is rebound to <char> by common_allocator_rebind.
    If the target is not pointer-like (including nullptr, PTMF, reference_wrapper), the erasure contains another allocator (re-)bound to target_type.
    The internal <char> rebind is always updated after target construction or destruction, even if it's not Assignable. Here's the process:
    1. Rebind the given allocator to the new target type. This may be the saved allocator, a propagating one, or a default-constructed one.
    2. Construct the target with the correctly bound allocator object, so the wrapper and the erasure use that and not a copy.
    3. Construct, assign, or reconstruct the saved (<char>-bound) allocator from the used-in-place (target-bound) one.
    When reconstruction occurs, it always preserves value up to equality.  */
template< typename target_policy, typename allocator_in, typename ... sig >
class container_wrapper
    : public wrapper< target_policy, sig ... > // The wrapper constructor may determine the allocator value, so this comes first.
    , public allocator_in { // This is already rebound to <char>, unlike allocator_type.
    typedef std::allocator_traits< allocator_in > allocator_traits;
    typedef typename container_wrapper::wrapper wrapper;
    
    template< typename source, typename = void >
    struct rebind_allocator_for_source
        { typedef typename allocator_traits::template rebind_alloc< typename std::decay< source >::type > type; };
    template< typename source >
    struct rebind_allocator_for_source< source,
        typename std::enable_if< wrapper::template is_compatibly_wrapped< typename std::decay< source >::type >::value >::type >
        { typedef allocator_in type; };
    
    template< typename target_allocator = allocator_in, typename propagate = std::false_type, typename source >
    void assign( std::false_type, source && s ) {
        target_allocator && alloc = select_allocator( propagate{}, & saved_allocator(), & s ); // May be a temporary if not propagating.
        wrapper temp( std::allocator_arg, std::move( alloc ), std::forward< source >( s ) ); // Use referent of alloc in-place.
        assign_allocator( std::integral_constant< bool, propagate::value // Adopt updated allocator value if propagating,
                || ( ! std::is_same< allocator_in, target_allocator >::value // or if a temporary was used…
                    && std::is_move_assignable< allocator_in >::value ) >{}, // … and it supports assignment. (Duck typing alert!)
            & saved_allocator(), & alloc ); // Allocator assignment might throw; nothing forbids it. Avoid corruption in that case.
        wrapper::operator = ( std::move( temp ) ); // Does not throw.
    }
    template< typename = allocator_in, typename propagate = std::false_type, typename source >
    void assign( std::true_type, source && s ) noexcept { // Source is a compatible container_wrapper rvalue or it is pointer-like.
        assign_allocator( propagate{}, & saved_allocator(), & s ); // Terminate upon throwing allocator move assignment.
        wrapper::operator = ( std::forward< source >( s ) ); // Nothing aside from propagation can modify the allocator.
    }
    
protected:
    template< typename target >
    using is_targetable = typename target_policy::template temp< target >;
    template< typename source >
    using is_compatibly_wrapped = typename wrapper::template is_compatibly_wrapped< source >;
    
    template< typename source, typename = allocator_in >
    struct has_compatible_allocator : std::false_type {}; // Only valid given is_compatibly_wrapped.
    template< typename source >
    struct has_compatible_allocator< source, typename source::UGLY(common_allocator) > : std::true_type {};
    
    typedef std::integral_constant< bool,
        allocator_traits::propagate_on_container_move_assignment::value || MSVC_FIX (impl::) is_always_equal< allocator_in >::value > noexcept_move_assign;
public:
    allocator_in & saved_allocator() { return * this; } // Pseudo private, not const, and return type is not allocator_type.
    allocator_in const & saved_allocator() const { return * this; }
    
    typedef allocator_in UGLY(common_allocator);
    
    container_wrapper() = default;
    explicit container_wrapper( allocator_in const & alloc ) noexcept
        : allocator_in{ alloc } {}
    deprecated( "std::allocator_arg is now unnecessary in function_container classes. Just pass the allocator instead." )
    container_wrapper( std::allocator_arg_t, allocator_in const & in_alloc ) noexcept
        : UGLY(common_allocator){ in_alloc } {}
    
    container_wrapper( container_wrapper && s ) noexcept
        : container_wrapper( static_cast< wrapper && >( s ), std::move( s.saved_allocator() ) ) {}
    container_wrapper( container_wrapper const & s )
        : container_wrapper( static_cast< wrapper const & >( s ), allocator_traits::select_on_container_copy_construction( s.saved_allocator() ) ) {}
    
    template< typename source,
        typename std::enable_if< is_targetable< typename std::decay< source >::type >::value >::type * = nullptr >
    container_wrapper( source && s, typename rebind_allocator_for_source< source >::type alloc = {} )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value
            || ( is_compatibly_wrapped< source >::value && MSVC_FIX (impl::) is_always_equal< allocator_in >::value ) )
        : wrapper( std::allocator_arg, /* not invalidating */ std::move( alloc ), std::forward< source >( s ) )
        , allocator_in{ std::move( alloc ) } {}
    
    template< typename source, typename ... arg,
        typename std::enable_if< is_targetable< typename std::decay< source >::type >::value >::type * = nullptr >
    container_wrapper( std::allocator_arg_t, typename rebind_allocator_for_source< source >::type alloc, in_place_t< source > t, arg && ... a )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value )
        : wrapper( std::allocator_arg, std::move( alloc ), t, std::forward< arg >( a ) ... )
        , allocator_in{ std::move( alloc ) } {}
    
    template< typename source, typename ... arg,
        typename std::enable_if< is_targetable< typename std::decay< source >::type >::value >::type * = nullptr >
    container_wrapper( in_place_t< source > t, arg && ... a )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value )
        : container_wrapper( std::allocator_arg, {}, t, std::forward< arg >( a ) ... ) {}
    
    container_wrapper & operator = ( container_wrapper && s ) = delete; // Always assign [unique_]function_container instead, for conversion to unique.
    container_wrapper & operator = ( container_wrapper const & s ) = delete;
    
    template< typename source >
    typename std::enable_if< conjunction<
        is_compatibly_wrapped< source > // Incidentally check that source is not a reference.
        , has_compatible_allocator< source >
    >::value >::type
    operator = ( source && s )
    noexcept( noexcept_move_assign::value ) {
        typedef typename allocator_traits::propagate_on_container_move_assignment pocma;
        return noexcept_move_assign::value || saved_allocator() == s.saved_allocator()
            ? assign< allocator_in, pocma >( std::true_type{}, std::move( s ) )
            : assign< allocator_in, pocma >( noexcept_move_assign{}, std::move( s ) ); // If noexcept_move_assign, this is a dead branch and a non-instantiation.
    }
    template< typename source >
    typename std::enable_if< conjunction<
        is_compatibly_wrapped< source >
        , has_compatible_allocator< source >
    >::value >::type
    operator = ( source const & s )
        { return assign< allocator_in, typename allocator_traits::propagate_on_container_copy_assignment >( std::false_type{}, s ); }
    
    template< typename source >
    typename std::enable_if< conjunction<
        is_compatibly_wrapped< source > // Filter out lvalue reference sources.
        , negation< has_compatible_allocator< source > > // No compatible allocator typically indicates a non-container wrapper source.
    >::value >::type
    operator = ( source && s ) { // Throws allocator_mismatch_error.
        return this->nontrivial_target( s, & saved_allocator() ) // Verify dynamic allocator compatibility even if is_always_equal.
            && ! MSVC_FIX (impl::) is_always_equal< allocator_in >::value // POCMA doesn't apply here.
            ? assign( typename MSVC_FIX (impl::) is_always_equal< allocator_in >::type{}, std::move( s ) ) // If is_always_equal, this is a dead branch and a non-instantiation.
            : assign( std::true_type{}, std::move( s ) );
    }
    template< typename source >
    typename std::enable_if< conjunction<
        is_compatibly_wrapped< source >
        , negation< has_compatible_allocator< source > >
    >::value >::type
    operator = ( source const & s ) {
        return this->nontrivial_target( s, & saved_allocator() )?
            assign( std::false_type{}, s )
            : assign( std::true_type{}, s ); // POCCA doesn't apply here.
    }
    
    template< typename source >
    typename std::enable_if< ! is_compatibly_wrapped< typename std::decay< source >::type >::value >::type
    operator = ( source && s )
    noexcept( is_noexcept_erasable< typename std::decay< source >::type >::value ) {
        assign< typename allocator_traits::template rebind_alloc< typename std::decay< source >::type > >
            ( typename is_noexcept_erasable< typename std::decay< source >::type >::type{}, std::forward< source >( s ) );
    }
    
    template< typename target, typename ... arg >
    void emplace_assign( arg && ... a )
        { wrapper::template allocate_assign< target >( saved_allocator(), std::forward< arg >( a ) ... ); }
    
    void swap( container_wrapper & o ) noexcept {
        allocator_swap( typename allocator_traits::propagate_on_container_swap{}, & saved_allocator(), & o.saved_allocator() );
        wrapper::swap( o );
    }
};


#undef DISPATCH_CQ
#undef DISPATCH_CV
#undef DISPATCH_CVREFQ
#undef DISPATCH_CVOBJQ
#undef DISPATCH_ALL
#undef DISPATCH_TABLE
}

// The actual classes all just pull their interfaces out of private inheritance.
template< typename ... sig >
class function
    : impl::wrapper< impl::is_copyable_all_callable< function< sig ... >, sig ... >, sig ... > {
    template< typename, typename ... >
    friend class impl::wrapper;
    typedef typename function::wrapper wrapper;
public:
    typedef function UGLY(wrapper_type);
    MSVC_FIX( typedef typename wrapper::wrapper_base wrapper_base; ) // Help MSVC with name lookup through inheritance and friendship.
    using wrapper::wrapper;
    
    function() noexcept = default; // Investigate why these are needed. Compiler bug?
    function( function && s ) noexcept = default;
    function( function const & ) = default;
    
    function & operator = ( function && o ) noexcept {
        if ( & o != this ) wrapper::operator = ( static_cast< wrapper && >( o ) );
        return * this;
    }
    function & operator = ( function const & o ) {
        if ( & o != this ) wrapper::operator = ( static_cast< wrapper const & >( o ) );
        return * this;
    }
    function & operator = ( function & s )
        { return * this = static_cast< function const & >( s ); }
    
    template< typename source >
    typename std::enable_if< std::is_constructible< wrapper, source >::value,
    function & >::type operator = ( source && s )
    noexcept(noexcept( std::declval< wrapper & >() = std::declval< source >() ))
        { wrapper::operator = ( std::forward< source >( s ) ); return * this; }
    
    using wrapper::operator ();
    using wrapper::swap;
    using wrapper::target;
    using wrapper::target_type;
    using wrapper::verify_type;
    using wrapper::operator bool;
    using wrapper::complete_object_address;
    
    using wrapper::assign;
    using wrapper::emplace_assign;
    using wrapper::allocate_assign;
    // No allocator_type or get_allocator.
};

template< typename ... sig >
class unique_function
    : impl::wrapper< impl::is_all_callable< sig ... >, sig ... > {
    template< typename, typename ... >
    friend class impl::wrapper;
    typedef typename unique_function::wrapper wrapper;
public:
    typedef unique_function UGLY(wrapper_type);
    MSVC_FIX( typedef typename wrapper::wrapper_base wrapper_base; )
    using wrapper::wrapper;
    
    unique_function() noexcept = default;
    unique_function( unique_function && s ) noexcept = default;
    unique_function( unique_function const & ) = delete;
    
    unique_function & operator = ( unique_function && o ) noexcept {
        if ( & o != this ) wrapper::operator = ( static_cast< wrapper && >( o ) );
        return * this;
    }
    unique_function & operator = ( unique_function const & o ) = delete;
    
    template< typename source >
    typename std::enable_if< std::is_constructible< wrapper, source >::value,
    unique_function & >::type operator = ( source && s )
    noexcept(noexcept( std::declval< wrapper & >() = std::declval< source >() ))
        { wrapper::operator = ( std::forward< source >( s ) ); return * this; }
    
    using wrapper::operator ();
    using wrapper::swap;
    using wrapper::target;
    using wrapper::target_type;
    using wrapper::verify_type;
    using wrapper::operator bool;
    using wrapper::complete_object_address;
    
    using wrapper::assign;
    using wrapper::emplace_assign;
    using wrapper::allocate_assign;
    // No allocator_type or get_allocator.
};

template< typename allocator_in, typename ... sig >
class function_container
    : impl::container_wrapper< impl::is_copyable_all_callable< function_container< allocator_in, sig ... >, sig ... >,
        impl::common_allocator_rebind< allocator_in >, sig ... > {
    template< typename, typename ... >
    friend class impl::wrapper;
    template< typename, typename, typename ... >
    friend class impl::container_wrapper;
    
    typedef typename function_container::container_wrapper wrapper;
public:
    typedef function_container UGLY(wrapper_type);
    MSVC_FIX( typedef typename wrapper::wrapper_base wrapper_base; )
    using MSVC_FIX (UGLY(common_allocator) =) typename function_container::wrapper::UGLY(common_allocator);
    
    function_container() = default; // Investigate why these are needed. Compiler bug?
    function_container( function_container && s ) noexcept = default;
    function_container( function_container const & ) = default;
    
    explicit function_container( allocator_in const & alloc ) noexcept
        : wrapper( alloc ) {}
    
    template< typename source, typename std::enable_if<
        wrapper::template is_targetable< typename std::decay< source >::type >::value
    >::type * = nullptr >
    function_container( source && s )
    noexcept( std::is_nothrow_constructible< wrapper, source && >::value )
        : wrapper{ std::forward< source >( s ) } {}
    
    template< typename source, typename std::enable_if<
        wrapper::template is_targetable< typename std::decay< source >::type >::value
    >::type * = nullptr >
    function_container( source && s, allocator_in const & alloc )
    noexcept( std::is_nothrow_constructible< wrapper, source &&, allocator_in const & >::value )
        : wrapper( std::forward< source >( s ), alloc ) {}
    
    function_container & operator = ( function_container && s )
    noexcept ( wrapper::noexcept_move_assign::value ) {
        if ( & s != this ) wrapper::operator = ( std::move( s ) );
        return * this;
    }
    function_container & operator = ( function_container const & s ) {
        if ( & s != this ) wrapper::operator = ( s );
        return * this;
    }
    function_container & operator = ( function_container & s )
        { return * this = static_cast< function_container const & >( s ); }
    
    template< typename source >
    typename std::enable_if< wrapper::template is_targetable< typename std::decay< source >::type >::value,
    function_container & >::type operator = ( source && s )
    noexcept(noexcept( std::declval< wrapper & >() = std::declval< source >() ))
        { wrapper::operator = ( std::forward< source >( s ) ); return * this; }
    
    using wrapper::emplace_assign;
    
    using wrapper::operator ();
    void swap( function_container & o ) { return wrapper::swap( o ); }
    using wrapper::target;
    using wrapper::target_type;
    using wrapper::verify_type;
    using wrapper::operator bool;
    using wrapper::complete_object_address;
    
    typedef allocator_in allocator_type;
    allocator_type get_allocator() const { return * this; } // wrapper inherits from common allocator.
};

template< typename allocator_in, typename ... sig >
class unique_function_container
    : impl::container_wrapper< impl::is_all_callable< sig ... >, impl::common_allocator_rebind< allocator_in >, sig ... > {
    template< typename, typename ... >
    friend class impl::wrapper;
    template< typename, typename, typename ... >
    friend class impl::container_wrapper;
    
    typedef typename unique_function_container::container_wrapper wrapper;
public:
    typedef unique_function_container UGLY(wrapper_type);
    MSVC_FIX( typedef typename wrapper::wrapper_base wrapper_base; )
    using MSVC_FIX (UGLY(common_allocator) =) typename unique_function_container::wrapper::UGLY(common_allocator);
    
    unique_function_container() = default;
    unique_function_container( unique_function_container && s ) noexcept = default;
    unique_function_container( unique_function_container const & ) = delete;
    
    template< typename source, typename std::enable_if<
        wrapper::template is_targetable< typename std::decay< source >::type >::value
    >::type * = nullptr >
    unique_function_container( source && s )
    noexcept( std::is_nothrow_constructible< wrapper, source && >::value )
        : wrapper{ std::forward< source >( s ) } {}
    
    template< typename source, typename std::enable_if<
        wrapper::template is_targetable< typename std::decay< source >::type >::value
    >::type * = nullptr >
    unique_function_container( source && s, allocator_in const & alloc )
    noexcept( std::is_nothrow_constructible< wrapper, source &&, allocator_in const & >::value )
        : wrapper( std::forward< source >( s ), alloc ) {}
    
    unique_function_container & operator = ( unique_function_container && s )
    noexcept ( wrapper::noexcept_move_assign::value ) {
        if ( & s != this ) wrapper::operator = ( std::move( s ) );
        return * this;
    }
    unique_function_container & operator = ( unique_function_container const & s ) = delete;
    
    template< typename source >
    typename std::enable_if< wrapper::template is_targetable< typename std::decay< source >::type >::value,
    unique_function_container & >::type operator = ( source && s )
    noexcept(noexcept( std::declval< wrapper & >() = std::declval< source >() ))
        { wrapper::operator = ( std::forward< source >( s ) ); return * this; }
    
    using wrapper::operator ();
    using wrapper::swap;
    using wrapper::target;
    using wrapper::target_type;
    using wrapper::verify_type;
    using wrapper::operator bool;
    using wrapper::complete_object_address;
    
    typedef allocator_in allocator_type;
    allocator_type get_allocator() const { return * this; } // container_wrapper inherits from common allocator.
    using wrapper::emplace_assign;
    // No assign or allocate_assign.
};

#define DEFINE_WRAPPER_OPS( NAME, ... ) \
template< __VA_ARGS__ > \
bool operator == ( UNPACK NAME const & a, std::nullptr_t ) \
    { return !a; } \
template< __VA_ARGS__ > \
bool operator != ( UNPACK NAME const & a, std::nullptr_t ) \
    { return (bool) a; } \
template< __VA_ARGS__ > \
bool operator == ( std::nullptr_t, UNPACK NAME const & a ) \
    { return !a; } \
template< __VA_ARGS__ > \
bool operator != ( std::nullptr_t, UNPACK NAME const & a ) \
    { return (bool) a; } \
template< __VA_ARGS__ > \
void swap( UNPACK NAME & lhs, UNPACK NAME & rhs ) \
    noexcept(noexcept( lhs.swap( rhs ) )) \
    { lhs.swap( rhs ); }

DEFINE_WRAPPER_OPS( ( function< sig ... > ), typename ... sig )
DEFINE_WRAPPER_OPS( ( unique_function< sig ... > ), typename ... sig )
DEFINE_WRAPPER_OPS( ( function_container< cont, sig ... > ), typename cont, typename ... sig )
DEFINE_WRAPPER_OPS( ( unique_function_container< cont, sig ... > ), typename cont, typename ... sig )
#undef DEFINE_WRAPPER_OPS

#if __cplusplus >= 201402 // Return type deduction really simplifies these.
// See proposal, "std::recover: undoing type erasure"

template< typename erasure >
void const volatile * recover_address( erasure & e, std::false_type )
    { return e.complete_object_address(); }

template< typename erasure >
void const volatile * recover_address( erasure & e, std::true_type )
    { return e.referent_address(); }

struct bad_type_recovery : std::exception
    { virtual char const * what() const noexcept override { return "An object was not found with its expected type."; } };

template< typename want, typename erasure_ref >
constexpr auto && recover( erasure_ref && e ) {
    typedef std::remove_reference_t< erasure_ref > erasure;
    typedef std::conditional_t< std::is_const< erasure >::value, want const, want > prop_const;
    typedef std::conditional_t< std::is_volatile< erasure >::value, prop_const volatile, prop_const > prop_cv;
    typedef std::conditional_t< std::is_lvalue_reference< erasure_ref >::value, prop_cv &, prop_cv && > prop_cvref;
    if ( e.template verify_type< want >() ) {
        return static_cast< prop_cvref >( * ( std::decay_t< want > * ) recover_address( e, std::is_reference< want >{} ) );
    } else throw bad_type_recovery{};
}
#endif

#undef UGLY
#undef is_trivially_move_constructible
#undef is_trivially_copy_constructible
#undef deprecated
#undef OLD_GCC_FIX
#undef OLD_GCC_SKIP
#undef MSVC_FIX
#undef MSVC_SKIP
#undef UNPACK
#undef IGNORE
}

#endif
