
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include <boost/context/all.hpp>

/*
 * grammar:
 *   P ---> E '\0'
 *   E ---> T {('+'|'-') T}
 *   T ---> S {('*'|'/') S}
 *   S ---> digit | '(' E ')'
 */
class Parser{
   char next;
   std::istream& is;
   std::function<void(char)> cb;

   char pull(){
        return std::char_traits<char>::to_char_type(is.get());
   }

   void scan(){
       do{
           next=pull();
       }
       while(isspace(next));
   }

public:
   Parser(std::istream& is_,std::function<void(char)> cb_) :
      next(), is(is_), cb(cb_)
    {}

   void run() {
      scan();
      E();
   }

private:
   void E(){
      T();
      while (next=='+'||next=='-'){
         cb(next);
         scan();
         T();
      }
   }

   void T(){
      S();
      while (next=='*'||next=='/'){
         cb(next);
         scan();
         S();
      }
   }

   void S(){
      if (isdigit(next)){
         cb(next);
         scan();
      }
      else if(next=='('){
         cb(next);
         scan();
         E();
         if (next==')'){
             cb(next);
             scan();
         }else{
             throw std::runtime_error("parsing failed");
         }
      }
      else{
         throw std::runtime_error("parsing failed");
      }
   }
};

int main() {
    try {
        std::istringstream is("1+1");
        bool done=false;
        std::exception_ptr except;

        // create handle to main execution context
        auto sink(boost::context::execution_context::current());
        // execute parser in new execution context
        boost::context::execution_context source(
                [&sink,&is,&done,&except](void*){
                // create parser with callback function
                Parser p(is,
                         [&sink](char ch){
                                // resume main execution context
                                sink(&ch);
                        });
                    try {
                        // start recursive parsing
                        p.run();
                    } catch (...) {
                        // store other exceptions in exception-pointer
                        except = std::current_exception();
                    }
                    // set termination flag
                    done=true;
                    // resume main execution context
                    sink();
                });

        // user-code pulls parsed data from parser
        // invert control flow
        void* vp = source();
        if (except) {
            std::rethrow_exception(except);
        }
        while( ! done) {
            printf("Parsed: %c\n",* static_cast<char*>(vp));
            vp = source();
            if (except) {
                std::rethrow_exception(except);
            }
        }
        std::cout << "main: done" << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    return EXIT_FAILURE;
}
