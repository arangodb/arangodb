
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <boost/context/all.hpp>
#include <boost/lexical_cast.hpp>

class X{
private:
    std::exception_ptr excptr_;
    boost::context::execution_context caller_;
    boost::context::execution_context callee_;

public:
    X():
        excptr_(),
        caller_(boost::context::execution_context::current()),
        callee_( [this]( void * vp){
                    try {
                        int i = * static_cast< int * >( vp);
                        std::string str = boost::lexical_cast<std::string>(i);
                        caller_( & str);
                    } catch ( std::bad_cast const&) {
                        excptr_=std::current_exception();
                    }
                 })
    {}

    std::string operator()(int i){
        void * ret = callee_( & i);
        if(excptr_){
            std::rethrow_exception(excptr_);
        }
        return * static_cast< std::string * >( ret);
    }
};

int main() {
    X x;
    std::cout<<x(7)<<std::endl;
    std::cout << "done" << std::endl;
    return EXIT_SUCCESS;
}
