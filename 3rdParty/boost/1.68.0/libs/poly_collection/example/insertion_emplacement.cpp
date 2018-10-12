/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* examples of insertion and emplacement */

#include <algorithm>
#include <array>
#include <boost/poly_collection/base_collection.hpp>
#include <random>
#include "rolegame.hpp"

int main()
{
  boost::base_collection<sprite> c;

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

//[insertion_emplacement_1]
  c.insert(c.begin(),juggernaut{8});
//]
  render(c);

//[insertion_emplacement_2]
  c.insert(c.begin(),goblin{9});
//]
  render(c);

//[insertion_emplacement_3]
  c.insert(c.begin<juggernaut>()+2,juggernaut{10});
                             // ^^ remember local iterators are random access
//]
  render(c);

//[insertion_emplacement_4]
//=  c.insert(c.begin(typeid(warrior)),juggernaut{11}); // undefined behavior!!
//]

//[insertion_emplacement_5]
  boost::base_collection<sprite> c2;

  c2.insert(c.begin(),c.end()); // read below
//<-
  render(c2);
//->

  // add some more warriors
  std::array<warrior,3> aw={{11,12,13}};
  c2.insert(aw.begin(),aw.end());
//<-
  render(c2);
//->

  // add some goblins at the beginning of their segment
  std::array<goblin,3> ag={{14,15,16}};
  c2.insert(c2.begin<goblin>(),ag.begin(),ag.end());
//<-
  render(c2);
//->
//]

//[insertion_emplacement_6]
//<-
  // same as line at beginning of previous snippet
//->
  c2.insert(c.begin(),c.end());
//]

//[insertion_emplacement_7]
//=  c.emplace(11); // does not compile
//]

//[insertion_emplacement_8]
  c.emplace<goblin>(11); // now it works
//]
  render(c);

//[insertion_emplacement_9]
  c.emplace_hint<juggernaut>(c.begin(),12); // at the beginning if possible
  c.emplace_pos<goblin>(c.begin<goblin>()+2,13); // amidst the goblins
  c.emplace_pos<warrior>(c.begin(typeid(warrior)),14); // local_base_iterator
//]
  render(c);
}
