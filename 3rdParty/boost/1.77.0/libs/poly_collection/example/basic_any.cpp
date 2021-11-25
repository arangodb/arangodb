/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* basic usage of boost::any_collection */

#include <boost/poly_collection/any_collection.hpp>
#include <boost/type_erasure/operators.hpp>
#include <random>
#include "rolegame.hpp"

//[basic_any_1
std::ostream& operator<<(std::ostream& os,const sprite& s)
{
  s.render(os);
  return os;
}

// std::string already has a suitable operator<<

std::ostream& operator<<(std::ostream& os,const window& w)
{
  w.display(os);
  return os;
}
//]

int main()
{
//[basic_any_2
//=  #include <boost/poly_collection/any_collection.hpp>
//=  #include <boost/type_erasure/operators.hpp>
//=  ...
//=
  using renderable=boost::type_erasure::ostreamable<>;
  boost::any_collection<renderable> c;
//]

//[basic_any_3
  // populate with sprites
  std::mt19937                 gen{92748}; // some arbitrary random seed
  std::discrete_distribution<> rnd{{1,1,1}};
  for(int i=0;i<4;++i){        // assign each type with 1/3 probability
    switch(rnd(gen)){ 
      case 0: c.insert(warrior{i});break;
      case 1: c.insert(juggernaut{i});break;
      case 2: c.insert(goblin{i});break;
    }
  }

  // populate with messages
  c.insert(std::string{"\"stamina: 10,000\""});
  c.insert(std::string{"\"game over\""});

  // populate with windows
  c.insert(window{"pop-up 1"});
  c.insert(window{"pop-up 2"});
//]


//[basic_any_4
  const char* comma="";
  for(const auto& r:c){
    std::cout<<comma<<r;
    comma=",";
  }
  std::cout<<"\n";
//]
}
