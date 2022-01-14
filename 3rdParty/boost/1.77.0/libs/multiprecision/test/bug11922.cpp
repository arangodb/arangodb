///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>
#include <memory>

#if defined(__apple_build_version__) && (__clang_major__ < 9)
//
// Apples clang fails with:
// error: no matching function for call to '__implicit_conversion_to'
// Which is nothing to do with us really...
//
#define DISABLE_TEST
#endif

typedef boost::multiprecision::cpp_int mp_int;

#if !defined(DISABLE_TEST)

class Int1
{
 public:
   Int1(const mp_int& ) {}
   Int1(const Int1& ) {}
};

class Int2
{
 public:
   Int2(const mp_int& ) {}
   Int2(const Int2& ) = delete;
};

int main()
{
   using namespace boost::multiprecision;

   mp_int i(10);
   Int1   a(i + 10);
   Int2   b(i + 20);

   std::shared_ptr<Int1> p1 = std::make_shared<Int1>(i + 10);
   std::shared_ptr<Int2> p2 = std::make_shared<Int2>(i + 10);

   return 0;
}

#else

int main()
{
   return 0;
}

#endif
