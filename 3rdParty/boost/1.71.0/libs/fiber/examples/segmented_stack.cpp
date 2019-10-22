
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <thread>

#include <boost/assert.hpp>
#include <boost/fiber/all.hpp>

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

void foo()
{
    bar( count);
	boost::this_fiber::yield();
}

void thread_fn()
{
    {
        boost::fibers::fiber f(
#if defined(BOOST_USE_SEGMENTED_STACKS)
				std::allocator_arg,
				boost::fibers::segmented_stack(
					boost::fibers::segmented_stack::traits_type::default_size() ),
#endif
                                foo);
        f.join();
    }
}

int main( int argc, char * argv[])
{
#if defined(BOOST_USE_SEGMENTED_STACKS)
    std::cout << "using segmented_stack stacks: allocates " << count << " * 4kB == " << 4 * count << "kB on stack, ";
    std::cout << "initial stack size = " << boost::fibers::segmented_stack::traits_type::default_size() / 1024 << "kB" << std::endl;
    std::cout << "application should not fail" << std::endl;
#else
    std::cout << "using standard stacks: allocates " << count << " * 4kB == " << 4 * count << "kB on stack, ";
    std::cout << "initial stack size = " << boost::fibers::fixedsize_stack::traits_type::default_size() / 1024 << "kB" << std::endl;
    std::cout << "application might fail" << std::endl;
#endif

    std::thread( thread_fn).join();

	std::cout << "done." << std::endl;

    return 0;
}
