/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>

#ifndef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
int main(){}
#else

//[ remove_transaction_safe
#include <type_traits>
#include <boost/callable_traits/remove_transaction_safe.hpp>

namespace ct = boost::callable_traits;

using ts = int() transaction_safe;
using not_ts = int();
using ts_removed = ct::remove_transaction_safe_t<ts>;

static_assert(std::is_same<not_ts, ts_removed>{}, "");
    
int main() {}
//]
#endif //#ifndef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE

