#include <boost/fusion/include/deque.hpp>
#include <boost/fusion/include/make_deque.hpp>
#include <boost/fusion/sequence/intrinsic/at_c.hpp>

#define ZI_TUPLE      boost::fusion::deque
#define ZI_MAKE_TUPLE boost::fusion::make_deque
#define ZI_TUPLE_GET(n) boost::fusion::at_c<n>

#include "detail/zip_iterator_test_original.ipp"
