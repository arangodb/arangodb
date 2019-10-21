#include <boost/tuple/tuple.hpp>

#define ZI_TUPLE      boost::tuples::tuple
#define ZI_MAKE_TUPLE boost::make_tuple
#define ZI_TUPLE_GET(n) boost::tuples::get<n>
#define ZI_USE_BOOST_TUPLE

#include "detail/zip_iterator_test_original.ipp"
