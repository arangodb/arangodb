
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <boost/variant.hpp>
#include <boost/context/execution_context.hpp>
#include <boost/lexical_cast.hpp>

typedef boost::variant<int,std::string> variant_t;

namespace ctx = boost::context;

class X{
private:
    std::exception_ptr excptr_;
    ctx::execution_context<variant_t> ctx_;

public:
    X():
        excptr_(),
        ctx_(
             [this](ctx::execution_context<variant_t> && ctx, variant_t data){
                try {
                    for (;;) {
                        int i = boost::get<int>(data);
                        data = boost::lexical_cast<std::string>(i);
                        auto result = ctx( data);
                        ctx = std::move( std::get<0>( result) );
                        data = std::get<1>( result);
                    }
                } catch ( std::bad_cast const&) {
                    excptr_=std::current_exception();
                }
                return std::move( ctx);
             })
    {}

    std::string operator()(int i){
        variant_t data = i;
        auto result = ctx_( data);
        ctx_ = std::move( std::get<0>( result) );
        data = std::get<1>( result);
        if(excptr_){
            std::rethrow_exception(excptr_);
        }
        return boost::get<std::string>(data);
    }
};

int main() {
    X x;
    std::cout<<x(7)<<std::endl;
    std::cout << "done" << std::endl;
}
