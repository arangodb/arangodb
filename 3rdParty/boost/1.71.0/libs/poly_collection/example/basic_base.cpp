/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* basic usage of boost::base_collection */

#include <algorithm>
#include <boost/poly_collection/base_collection.hpp>
#include <random>
#include "rolegame.hpp"

int main()
{
//[basic_base_1
//=  #include <boost/poly_collection/base_collection.hpp>
//=  ...
//=
  boost::base_collection<sprite> c;

  std::mt19937                 gen{92748}; // some arbitrary random seed
  std::discrete_distribution<> rnd{{1,1,1}};
  for(int i=0;i<8;++i){        // assign each type with 1/3 probability
    switch(rnd(gen)){ 
      case 0: c.insert(warrior{i});break;
      case 1: c.insert(juggernaut{i});break;
      case 2: c.insert(goblin{i});break;
    }
  }
//]

  auto render=[&](){
//[basic_base_2
    const char* comma="";
    for(const sprite& s:c){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    }
    std::cout<<"\n";
//]
  };
  render();

//[basic_base_3
  c.insert(goblin{8});
//]
  render();

//[basic_base_4
  // find element with id==7 and remove it
  auto it=std::find_if(c.begin(),c.end(),[](const sprite& s){return s.id==7;});
  c.erase(it);
//]
  render();
}
