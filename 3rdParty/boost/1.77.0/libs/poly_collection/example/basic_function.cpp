/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* basic usage of boost::function_collection */

#include <boost/poly_collection/function_collection.hpp>
#include <functional>
#include <memory>
#include <random>
#include <vector>
#include "rolegame.hpp"

int main()
{
//[basic_function_1
  std::vector<std::unique_ptr<sprite>> sprs;
  std::vector<std::string>             msgs;
  std::vector<window>                  wnds;
//]

  // populate sprs
  std::mt19937                 gen{92748}; // some arbitrary random seed
  std::discrete_distribution<> rnd{{1,1,1}};
  for(int i=0;i<4;++i){        // assign each type with 1/3 probability
    switch(rnd(gen)){ 
      case 0: sprs.push_back(std::make_unique<warrior>(i));;break;
      case 1: sprs.push_back(std::make_unique<juggernaut>(i));break;
      case 2: sprs.push_back(std::make_unique<goblin>(i));break;
    }
  }

  // populate msgs
  msgs.push_back("\"stamina: 10,000\"");
  msgs.push_back("\"game over\"");

  // populate wnds
  wnds.emplace_back("pop-up 1");
  wnds.emplace_back("pop-up 2");

//[basic_function_2
//=  #include <boost/poly_collection/function_collection.hpp>
//=  ...
//=
  // function signature accepting std::ostream& and returning nothing
  using render_callback=void(std::ostream&);
   
  boost::function_collection<render_callback> c;
//]

//[basic_function_3
//<-
  auto render_sprite=[](const sprite& s){
//->
//=  auto render_sprite(const sprite& s){
    return [&](std::ostream& os){s.render(os);};
  }/*<-*/;/*->*/

//<-
  auto render_message=[](const std::string& m){
//->
//=  auto render_message(const std::string& m){
    return [&](std::ostream& os){os<<m;};
  }/*<-*/;/*->*/

//<-
  auto render_window=[](const window& w){
//->
//=  auto render_window(const window& w){
    return [&](std::ostream& os){w.display(os);};
  }/*<-*/;/*->*/
//=  ...
//=
  for(const auto& ps:sprs)c.insert(render_sprite(*ps)); 
  for(const auto& m:msgs)c.insert(render_message(m));
  for(const auto& w:wnds)c.insert(render_window(w)); 
//]

//[basic_function_4
  const char* comma="";
  for(const auto& cbk:c){
    std::cout<<comma;
    cbk(std::cout);
    comma=",";
  }
  std::cout<<"\n";
//]

//[basic_function_5
  auto cbk=*c.begin();
  cbk(std::cout); // renders first element to std::cout
  std::function<render_callback> f=cbk;
  f(std::cout);   // exactly the same
//]

//[basic_function_6
//=  *c.begin()=render_message("last minute message"); // compile-time error
//]
}
