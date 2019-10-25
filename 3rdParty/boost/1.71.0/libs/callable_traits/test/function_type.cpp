
#include <boost/callable_traits/function_type.hpp>
#include "test.hpp"

int main() {

    struct foo;

    {
        auto g = [](int, char){};
        using G = decltype(g);
        using F = void(int, char);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
        CT_ASSERT(std::is_same<function_type_t<G const &>, F>::value);
        CT_ASSERT(std::is_same<function_type_t<G const &&>, F>::value);
        CT_ASSERT(std::is_same<function_type_t<G volatile &>, F>::value);
        CT_ASSERT(std::is_same<function_type_t<G &>, F>::value);
        CT_ASSERT(std::is_same<function_type_t<G &&>, F>::value);
        CT_ASSERT(std::is_same<function_type_t<
            decltype(&G::operator())>, void(G const &, int, char)>::value);
    }

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    {
        using G = void (int, char, ...) const LREF;
        using F = void (int, char, ...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }
#endif

    {
        using G = void (*)(int, char) BOOST_CLBL_TRTS_TRANSACTION_SAFE_SPECIFIER;
        using F = void (int, char);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (* const &)(int, char);
        using F = void (int, char);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = int (* &&)();
        using F = int ();
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = int (* &&)();
        using F = int ();
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = char const * const & (&)(...);
        using F = char const * const & (...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        struct G { int operator() (...) volatile; };
        using F = int (...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        struct G { int operator() (...) volatile BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER; };
        using F = int (...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        struct G { int operator() (...) const BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER; };
        using F = int (...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (foo::* const &)(int, int, int) LREF BOOST_CLBL_TRTS_TRANSACTION_SAFE_SPECIFIER;
        using F = void (foo &, int, int, int);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }
#ifndef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
    {
        using G = void (foo::* const &)(int, int, int) RREF;
        using F = void (foo &&, int, int, int);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }
#endif
    {
        using G = void (foo::*)(int, int, int) const BOOST_CLBL_TRTS_TRANSACTION_SAFE_SPECIFIER;
        using F = void (foo const &, int, int, int);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (foo::*)(int, int, int);
        using F = void (foo &, int, int, int);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (foo::*)() BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER;
        using F = void (foo &);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (foo::*)(int, int, int) const BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER;
        using F = void (foo const &, int, int, int);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }{
        using G = void (foo::*)(int, int, int, ...) const BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER;
        using F = void (foo const &, int, int, int, ...);
        CT_ASSERT(std::is_same<TRAIT(function_type, G), F>::value);
    }
}
