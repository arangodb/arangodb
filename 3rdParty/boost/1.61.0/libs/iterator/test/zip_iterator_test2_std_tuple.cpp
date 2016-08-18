#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_HDR_TUPLE)

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
