#include "visibility_test_lib.hpp"
#include <boost/throw_exception.hpp>

void
BOOST_SYMBOL_EXPORT
hidden_throw()
	{
	BOOST_THROW_EXCEPTION(my_exception() << my_info(42));
	}
