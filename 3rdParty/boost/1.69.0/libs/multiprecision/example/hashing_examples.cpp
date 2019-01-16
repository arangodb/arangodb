///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random.hpp>
#include <boost/functional/hash.hpp>
#include <unordered_set>
#include <city.h>

//[hash1

/*`
All of the types in this library support hashing via boost::hash or std::hash.
That means we can use multiprecision types directly in hashed containers such as std::unordered_set:
*/
//]

void t1()
{
   //[hash2
   using namespace boost::multiprecision;
   using namespace boost::random;

   mt19937 mt;
   uniform_int_distribution<uint256_t> ui;

   std::unordered_set<uint256_t> set;
   // Put 1000 random values into the container:
   for(unsigned i = 0; i < 1000; ++i)
      set.insert(ui(mt));

   //]
}

//[hash3

/*` 
Or we can define our own hash function, for example in this case based on
Google's CityHash:
*/

struct cityhash
{
   std::size_t operator()(const boost::multiprecision::uint256_t& val)const
   {
      // create a hash from all the limbs of the argument, this function is probably x64 specific,
      // and requires that we access the internals of the data type:
      std::size_t result = CityHash64(reinterpret_cast<const char*>(val.backend().limbs()), val.backend().size() * sizeof(val.backend().limbs()[0]));
      // modify the returned hash based on sign:
      return val < 0 ? ~result : result;
   }
};

//]

void t2()
{
//[hash4

/*`As before insert some values into a container, this time using our custom hasher:*/

   std::unordered_set<uint256_t, cityhash> set2;
   for(unsigned i = 0; i < 1000; ++i)
      set2.insert(ui(mt));

//]
}

int main()
{
   t1();
   t2();
   return 0;
}

