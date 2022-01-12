
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/coroutine/all.hpp>

#include <iostream>

#include <boost/assert.hpp>
#include <boost/config.hpp>

int count = 384;

#ifdef BOOST_MSVC //MS VisualStudio
__declspec(noinline) void access( char *buf);
#else // GCC
void access( char *buf) __attribute__ ((noinline));
#endif
void access( char *buf)
{
  buf[0] = '\0';
}

void bar( int i)
{
    char buf[4 * 1024];

    if ( i > 0)
    {
        access( buf);
        std::cout << i << ". iteration" << std::endl;
        bar( i - 1);
    }
}

void foo( boost::coroutines::asymmetric_coroutine< void >::pull_type & source)
{
    bar( count);
    source();
}


int main( int argc, char * argv[])
{
#if defined(BOOST_USE_SEGMENTED_STACKS)
    std::cout << "using segmented stacks: allocates " << count << " * 4kB == " << 4 * count << "kB on stack, ";
    std::cout << "initial stack size = " << boost::coroutines::stack_allocator::traits_type::default_size() / 1024 << "kB" << std::endl;
    std::cout << "application should not fail" << std::endl;
#else
    std::cout << "using standard stacks: allocates " << count << " * 4kB == " << 4 * count << "kB on stack, ";
    std::cout << "initial stack size = " << boost::coroutines::stack_allocator::traits_type::default_size() / 1024 << "kB" << std::endl;
    std::cout << "application might fail" << std::endl;
#endif

    boost::coroutines::asymmetric_coroutine< void >::push_type sink( foo);
    sink();

    std::cout << "Done" << std::endl;

    return 0;
}
