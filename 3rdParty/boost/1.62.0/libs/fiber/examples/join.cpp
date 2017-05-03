#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <string>

#include <boost/fiber/all.hpp>

int value1 = 0;
int value2 = 0;

void fn1()
{
    boost::fibers::fiber::id id = boost::this_fiber::get_id();
    for ( int i = 0; i < 5; ++i)
    {
        ++value1;
        std::cout << "fiber " << id << " fn1: increment value1: " << value1 << std::endl;
        boost::this_fiber::yield();
    }
    std::cout << "fiber " << id << " fn1: returns" << std::endl;
}

void fn2( boost::fibers::fiber & f)
{
    boost::fibers::fiber::id id = boost::this_fiber::get_id();
    for ( int i = 0; i < 5; ++i)
    {
        ++value2;
        std::cout << "fiber " << id << " fn2: increment value2: " << value2 << std::endl;
        if ( i == 1)
        {
            boost::fibers::fiber::id id = f.get_id();
            std::cout << "fiber " << id << " fn2: joins fiber " << id << std::endl;
            f.join();
            std::cout << "fiber " << id << " fn2: joined fiber " << id << std::endl;
        }
        boost::this_fiber::yield();
    }
    std::cout << "fiber " << id << " fn2: returns" << std::endl;
}

int main()
{
    try
    {
        boost::fibers::fiber f1( fn1);
        boost::fibers::fiber f2( fn2, std::ref( f1) );

        f2.join();

        std::cout << "done." << std::endl;

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
