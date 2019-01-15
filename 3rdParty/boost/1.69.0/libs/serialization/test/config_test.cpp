#include "../../../boost/config.hpp"

#if defined(__clang__)
#pragma message "__clang__ defined"
#endif

#if defined(BOOST_CLANG)
#pragma message "BOOST_CLANG defined"
#endif

#if defined(__GNUC__)
#pragma message "__GNUC__ defined"
#endif

#include "../../../boost/mpl/print.hpp"

typedef int x;
