
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cctype>
#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>

#include <boost/coroutine2/all.hpp>

class parser_error : public std::runtime_error {
public:
    parser_error() :
        std::runtime_error("parsing failed") {
    }
};

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
      if (std::isdigit(next)){
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
             throw parser_error();
         }
      }
      else{
         throw parser_error();
      }
   }
};

typedef boost::coroutines2::coroutine< char > coro_t;

int main() {
    try {
        std::istringstream is("1+1");
        // invert control flow
        coro_t::pull_type seq(
                [&is]( coro_t::push_type & yield) {
                Parser p( is,
                    [&yield](char ch){
                    yield(ch);
                    });
                p.run();
                });
        // user-code pulls parsed data from parser
        for(char c:seq){
            printf("Parsed: %c\n",c);
        }
        std::cout << "\nDone" << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& ex) {
        std::cerr << "exception: " << ex.what() << std::endl;
    }
    return EXIT_FAILURE;
}

