///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/multiprecision/cpp_int.hpp>
#include <memory>

typedef boost::multiprecision::cpp_int mp_int;

#if !defined(BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS) && !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

class Int1 {
public:
   Int1(const mp_int& i) {}
   Int1(const Int1& i) {}
};

class Int2 {
public:
   Int2(const mp_int& i) {}
   Int2(const Int2& i) = delete;
};


int main()
{
   using namespace boost::multiprecision;

   mp_int i(10);
   Int1 a(i + 10);
   Int2 b(i + 20);

#ifndef BOOST_NO_CXX11_SMART_PTR

   std::shared_ptr<Int1> p1 = std::make_shared<Int1>(i + 10);
   std::shared_ptr<Int2> p2 = std::make_shared<Int2>(i + 10);

#endif
   
   return 0;
}

#else

int main() { return 0; }

#endif