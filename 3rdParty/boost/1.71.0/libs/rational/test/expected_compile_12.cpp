///////////////////////////////////////////////////////////////////////////////
//  Copyright 2019. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information read https://github.com/boostorg/rational/issues/26

#include <boost/rational.hpp>

void test(const char* text)
{
	(void)text;
}

void test(const boost::rational<int>& rational)
{
	(void)rational;
}

int main()
{
	test("Some text");
	return 0;
}
