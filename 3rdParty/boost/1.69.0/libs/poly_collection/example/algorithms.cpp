/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

/* Boost.PolyCollection algorithms */

#include <algorithm>
#include <boost/poly_collection/algorithm.hpp>
#include <boost/poly_collection/any_collection.hpp>
#include <boost/poly_collection/base_collection.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/operators.hpp>
#include <boost/type_erasure/typeid_of.hpp>
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

  auto render1=[](const boost::base_collection<sprite>& c){
//[algorithms_1
    const char* comma="";
    std::for_each(c.begin(),c.end(),[&](const sprite& s){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    });
    std::cout<<"\n";
//]
  };
  render1(c);

  auto render2=[](const boost::base_collection<sprite>& c){
//[algorithms_2
    const char* comma="";
    for(auto seg_info:c.segment_traversal()){
      for(const sprite& s:seg_info){
        std::cout<<comma;
        s.render(std::cout);
        comma=",";
      }
    }
    std::cout<<"\n";
//]
  };
  render2(c);

  auto render3=[](const boost::base_collection<sprite>& c){
//[algorithms_3
//=    #include <boost/poly_collection/algorithm.hpp>
//=    ...
//=
    const char* comma="";
    boost::poly_collection::for_each(c.begin(),c.end(),[&](const sprite& s){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    });
    std::cout<<"\n";
//]
  };
  render3(c);

//[algorithms_4
  auto n=boost::poly_collection::count_if(
    c.begin(),c.end(),[](const sprite& s){return s.id%2==0;});
  std::cout<<n<<" sprites with even id\n";
//]

  using renderable=boost::type_erasure::ostreamable<>;
  using standalone_renderable=boost::mpl::vector<
    renderable,
    boost::type_erasure::copy_constructible<>,
    boost::type_erasure::typeid_<>
  >;

  {
//[algorithms_5
    sprite*  ps=new warrior{5};
    // sprite -> warrior
    warrior* pw=static_cast<warrior*>(ps);
//<-
    (void)pw;
    delete ps;
//->

//<-
    boost::type_erasure::any<standalone_renderable> r=std::string{"hello"};
//->
//=    boost::type_erasure::any<renderable> r=std::string{"hello"};
    // renderable -> std::string
    std::string& str=boost::type_erasure::any_cast<std::string&>(r);
//]

//[algorithms_6
    // render r with std::string restitution
    if(boost::type_erasure::typeid_of(r)==typeid(std::string)){
      std::string& str=boost::type_erasure::any_cast<std::string&>(r);
      std::cout<<str<<"\n";
    }
    else{
      std::cout<<r<<"\n";
    }
//]
  }

  auto& bc=c;
  {
    boost::any_collection<renderable> c;
    c.insert(bc.begin<warrior>(),bc.end<warrior>());
    c.insert(bc.begin<juggernaut>(),bc.end<juggernaut>());
    c.insert(bc.begin<goblin>(),bc.end<goblin>());
    c.insert(std::string{"\"stamina: 10,000\""});
    c.insert(std::string{"\"game over\""});
    c.insert(window{"pop-up 1"});
    c.insert(window{"pop-up 2"});

//[algorithms_7
    const char* comma="";
    boost::poly_collection::for_each
      <warrior,juggernaut,goblin>( // restituted types
      c.begin(),c.end(),[&](const auto& x){ // loop traverses *all* elements
        std::cout<<comma<<x;
        comma=",";
      });
    std::cout<<"\n";
//]
  }

//[algorithms_8
  const char* comma="";
  boost::poly_collection::for_each<warrior,juggernaut,goblin>(
    c.begin(),c.end(),[&](const auto& s){
      std::cout<<comma;
      s.render(std::cout);
      comma=",";
    });
  std::cout<<"\n";
//]
}
