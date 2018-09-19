/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* segment-specific operations */

#include <algorithm>
#include <boost/poly_collection/any_collection.hpp>
#include <boost/poly_collection/base_collection.hpp>
#include <boost/type_erasure/operators.hpp>
#include <memory>
#include <random>
#include "rolegame.hpp"

std::ostream& operator<<(std::ostream& os,const sprite& s)
{
  s.render(os);
  return os;
}

std::ostream& operator<<(std::ostream& os,const window& w)
{
  w.display(os);
  return os;
}

int main()
{
  boost::base_collection<sprite> c;

//[segmented_structure_1
//=  std::unique_ptr<sprite> make_sprite()
//=  {
//<-
  auto make_sprite=[]()->std::unique_ptr<sprite>{
//->
    static std::mt19937                 gen{92748};
    static std::discrete_distribution<> rnd{{1,1,1}};
    static int                          id=0;

    switch(rnd(gen)){ 
//<-
      default:
//->
      case 0: return std::make_unique<warrior>(id++);break;
      case 1: return std::make_unique<juggernaut>(id++);break;
      case 2: return std::make_unique<goblin>(id++);break;
    }
//<-
  };
//->
//=  }
//=  ...
//=
//<-
  try{
//->
  for(int i=0;i<8;++i)c.insert(*make_sprite());
  // throws boost::poly_collection::unregistered_type
//<-
  }catch(boost::poly_collection::unregistered_type&){}
//->
//]

//[segmented_structure_2
  std::cout<<c.is_registered<warrior>()<<"\n";       // prints 0
  std::cout<<c.is_registered(typeid(warrior))<<"\n"; // alternate syntax
//]

//[segmented_structure_3
  c.register_types<warrior,juggernaut,goblin>();
  // everything works fine now
  for(int i=0;i<8;++i)c.insert(*make_sprite());
//]

  using renderable=boost::type_erasure::ostreamable<>;
//[segmented_structure_4
  boost::any_collection<renderable> c1,c2;
//=  ... // populate c2
//=
//<-
  c2.insert(window{"pop-up"});
  try{
//->
  c1.insert(*c2.begin()); // throws: actual type of *c2.begin() not known by c1
//<-
  }catch(boost::poly_collection::unregistered_type&){}
//->
//]

//[segmented_structure_5
//=  ... // populate c with 8 assorted entities
//=
  std::cout<<c.size()<<"\n";                    // 8 sprites
  std::cout<<c.size<juggernaut>()<<"\n";        // 2 juggernauts
  std::cout<<c.size(typeid(juggernaut))<<"\n";  // alternate syntax
  c.clear<juggernaut>();                        // remove juggenauts only
  std::cout<<c.empty<juggernaut>()<<"\n";       // 1 (no juggernauts left)
  std::cout<<c.size()<<"\n";                    // 6 sprites remaining
//]

//[segmented_structure_6
  const char* comma="";
  for(auto first=c.begin(typeid(warrior)),last=c.end(typeid(warrior));
      first!=last;++first){
    std::cout<<comma;
    first->render(std::cout);
    comma=",";
  }
  std::cout<<"\n";
//]

//[segmented_structure_7
  /*=const char**/ comma="";
  for(const auto& x:c.segment(typeid(warrior))){
    std::cout<<comma;
    x.render(std::cout);
    comma=",";
  }
  std::cout<<"\n";
//]

//[segmented_structure_8
  /*=const char**/ comma="";
  for(auto first=c.begin<warrior>(),last=c.end<warrior>();
      first!=last;++first){
    first->rank.insert(0,"super");
    std::cout<<comma;
    first->render(std::cout);
    comma=",";
  }
  std::cout<<"\n";

  // range-based for loop alternative

  /*=const char**/ comma="";
  for(auto& x:c.segment<warrior>()){
    x.rank.insert(0,"super");
//<-
    auto it=x.rank.begin();
    x.rank.erase(it,it+5); // undo previos op, 5==len("super");
//->
    std::cout<<comma;
    x.render(std::cout);
    comma=",";
  }
  std::cout<<"\n";

//]

  auto render=[&](){
//[segmented_structure_9
  const char* comma="";
  for(auto seg:c.segment_traversal()){
    for(sprite& s:seg){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    }
  }
  std::cout<<"\n";
//]
  };
  render();

//[segmented_structure_10
  c.reserve<goblin>(100); // no reallocation till we exceed 100 goblins
  std::cout<<c.capacity<goblin>()<<"\n"; // prints 100
//]

//[segmented_structure_11
  c.reserve(1000); // reserve(1000) for each segment
  std::cout<<c.capacity<warrior>()<<", "
           <<c.capacity<juggernaut>()<<", "
           <<c.capacity<goblin>()<<"\n"; // prints 1000, 1000, 1000
//]
}
