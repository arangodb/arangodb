#include <boost/config.hpp>

// libstdc++ from gcc 4.4 has a broken std::tuple that cannot be constructed from a compatible tuple,
// e.g. std::tuple<int, double> from std::tuple<int const&, double const&>.
#if !defined(BOOST_NO_CXX11_HDR_TUPLE) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && \
    (!defined(BOOST_LIBSTDCXX_VERSION) || BOOST_LIBSTDCXX_VERSION >= 40500)

#include <tuple>
#include <boost/fusion/adapted/std_tuple.hpp>

#define ZI_TUPLE      std::tuple
#define ZI_MAKE_TUPLE std::make_tuple
#define ZI_TUPLE_GET(n) std::get<n>

#include "detail/zip_iterator_test_original.ipp"

#else

int main()
{
    return 0;
}

#endif
