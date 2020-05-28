// Copyright (C) 2017 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

boost::optional<int> getitem();

int main(int argc, const char *[])
{
  boost::optional<int> a = getitem();
  boost::optional<int> b;

  if (argc > 0)
    b = argc;

  if (a == b)
    return 1;

  return 0;
}