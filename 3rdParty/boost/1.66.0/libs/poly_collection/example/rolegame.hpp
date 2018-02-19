/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_EXAMPLE_ROLEGAME_HPP
#define BOOST_POLY_COLLECTION_EXAMPLE_ROLEGAME_HPP

#if defined(_MSC_VER)
#pragma once
#endif

/* entities of a purported role game used in the examples */

#include <iostream>
#include <string>
#include <utility>

//[rolegame_1
struct sprite
{
  sprite(int id):id{id}{}
  virtual ~sprite()=default;
  virtual void render(std::ostream& os)const=0;

  int id;
};
//]

//[rolegame_2
struct warrior:sprite
{
  using sprite::sprite;
  warrior(std::string rank,int id):sprite{id},rank{std::move(rank)}{}

  void render(std::ostream& os)const override{os<<rank<<" "<<id;}

  std::string rank="warrior";
};

struct juggernaut:warrior
{
  juggernaut(int id):warrior{"juggernaut",id}{}
};

struct goblin:sprite
{
  using sprite::sprite;
  void render(std::ostream& os)const override{os<<"goblin "<<id;}
};
//]

//[rolegame_3
struct window
{
  window(std::string caption):caption{std::move(caption)}{}

  void display(std::ostream& os)const{os<<"["<<caption<<"]";}

  std::string caption;
};
//]

//[rolegame_4
struct elf:sprite
{
  using sprite::sprite;
  elf(const elf&)=delete; // not copyable
  elf(elf&&)=default;     // but moveable
  void render(std::ostream& os)const override{os<<"elf "<<id;}
};
//]


#endif
