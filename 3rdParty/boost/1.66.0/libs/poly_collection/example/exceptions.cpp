/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* Boost.PolyCollection exceptions */

#include <algorithm>
#include <array>
#include <boost/poly_collection/base_collection.hpp>
#include <random>
#include "rolegame.hpp"

int main()
{
  boost::base_collection<sprite> c,c2;

  // populate c

  std::mt19937                 gen{92748}; // some arbitrary random seed
  std::discrete_distribution<> rnd{{1,1,1}};
  for(int i=0;i<8;++i){        // assign each type with 1/3 probability
    switch(rnd(gen)){ 
      case 0: c.insert(warrior{i});break;
      case 1: c.insert(juggernaut{i});break;
      case 2: c.insert(goblin{i});break;
    }
  }

  auto render=[](const boost::base_collection<sprite>& c){
    const char* comma="";
    for(const sprite& s:c){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    }
    std::cout<<"\n";
  };
  render(c);

  try{
//[exceptions_1
  c.insert(elf{0}); // no problem
//=  ...
//=
  c2=c; // throws boost::poly_collection::not_copy_constructible
//]
  }catch(boost::poly_collection::not_copy_constructible&){}

  try{
//[exceptions_2
  c.clear<elf>(); // get rid of non-copyable elfs
  c2=c;           // now it works
  // check that the two are indeed equal
  std::cout<<(c==c2)<<"\n";
                  // throws boost::poly_collection::not_equality_comparable
//]
  }catch(boost::poly_collection::not_equality_comparable&){}
}
