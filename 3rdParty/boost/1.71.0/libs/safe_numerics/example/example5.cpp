#include <stdexcept>
#include <iostream>
#include <array>

#include <boost/safe_numerics/safe_integer_range.hpp>

void detected_msg(bool detected){
    std::cout << (detected ? "error detected!" : "error NOT detected! ") << std::endl;
}

int main(int, const char *[]){
    // problem: array index values can exceed array bounds
    std::cout << "example 5: ";
    std::cout << "array index values can exceed array bounds" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    std::array<int, 37> i_array;

    // unsigned int i_index = 43;
    // the following corrupts memory.
    // This may or may not be detected at run time.
    // i_array[i_index] = 84; // comment this out so it can be tested!
    std::cout << "error NOT detected!" << std::endl;

    // solution: replace unsigned array index with safe_unsigned_range
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        using i_index_t = safe_unsigned_range<0, i_array.size() - 1>;
        i_index_t i_index;
        i_index = 36; // this works fine
        i_array[i_index] = 84;
        i_index = 43; // throw exception here!
        std::cout << "error NOT detected!" << std::endl; // so we never arrive here
    }
    catch(const std::exception & e){
        std::cout <<  "error detected:" << e.what() << std::endl;
    }
    return 0;
}
