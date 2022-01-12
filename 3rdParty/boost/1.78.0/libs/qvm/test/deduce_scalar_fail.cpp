//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/deduce_scalar.hpp>

template <class> struct tester { };
template <> struct tester<void>;

int char_scalars_not_supported_use_signed_char_instead = sizeof(tester<boost::qvm::deduce_scalar<char,int>::type>);
