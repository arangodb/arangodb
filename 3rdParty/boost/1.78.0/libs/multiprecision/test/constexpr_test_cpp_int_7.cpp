///////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

// Contains Quickbook snippets used by boost/libs/multiprecision/doc/multiprecision.qbk,
// used in section Literal Types and constexpr Support, last example on constexpr randoms.

// A implementation and demonstration of the Keep It Simple Stupid random number generator algorithm https://en.wikipedia.org/wiki/KISS_(algorithm) for cpp_int integers.
// b2 --abbreviate-paths toolset=clang-9.0.0 address-model=64 cxxstd=2a release misc > multiprecision_clang_misc.log

#include <boost/multiprecision/cpp_int.hpp>

#include <iostream>

struct kiss_rand
{
   typedef std::uint64_t result_type;

   constexpr kiss_rand() : x(0x8207ebe160468b32uLL), y(0x2871283e01d45bbduLL), z(0x9c80bfd5db9680c9uLL), c(0x2e2683c2abb878b8uLL) {}
   constexpr kiss_rand(std::uint64_t seed) : x(seed), y(0x2871283e01d45bbduLL), z(0x9c80bfd5db9680c9uLL), c(0x2e2683c2abb878b8uLL) {}
   constexpr kiss_rand(std::uint64_t seed_x, std::uint64_t seed_y) : x(seed_x), y(seed_y), z(0x9c80bfd5db9680c9uLL), c(0x2e2683c2abb878b8uLL) {}
   constexpr kiss_rand(std::uint64_t seed_x, std::uint64_t seed_y, std::uint64_t seed_z) : x(seed_x), y(seed_y), z(seed_z), c(0x2e2683c2abb878b8uLL) {}

   constexpr std::uint64_t operator()()
   {
      return MWC() + XSH() + CNG();
   }

 private:
   constexpr std::uint64_t MWC()
   {
      std::uint64_t t = (x << 58) + c;
      c               = (x >> 6);
      x += t;
      c += (x < t);
      return x;
   }
   constexpr std::uint64_t XSH()
   {
      y ^= (y << 13);
      y ^= (y >> 17);
      return y ^= (y << 43);
   }
   constexpr std::uint64_t CNG()
   {
      return z = 6906969069LL * z + 1234567;
   }
   std::uint64_t x, y, z, c;
};

inline constexpr void hash_combine(std::uint64_t& h, std::uint64_t k)
{
   constexpr const std::uint64_t m = 0xc6a4a7935bd1e995uLL;
   constexpr const int           r = 47;

   k *= m;
   k ^= k >> r;
   k *= m;

   h ^= k;
   h *= m;

   // Completely arbitrary number, to prevent 0's from hashing to 0.
   h += 0xe6546b64;
}

template <std::size_t N>
inline constexpr std::uint64_t string_to_hash(const char (&s)[N])
{
   std::uint64_t hash(0);
   for (unsigned i = 0; i < N; ++i)
      hash_combine(hash, s[i]);
   return hash;
}

template <class UnsignedInteger>
struct multiprecision_generator
{
   typedef UnsignedInteger result_type;
   constexpr               multiprecision_generator(std::uint64_t seed1) : m_gen64(seed1) {}
   constexpr               multiprecision_generator(std::uint64_t seed1, std::uint64_t seed2) : m_gen64(seed1, seed2) {}
   constexpr               multiprecision_generator(std::uint64_t seed1, std::uint64_t seed2, std::uint64_t seed3) : m_gen64(seed1, seed2, seed3) {}

   static constexpr result_type (min)()
   {
      return 0u;
   }
   static constexpr result_type (max)()
   {
      return ~result_type(0u);
   }
   constexpr result_type operator()()
   {
      result_type result(m_gen64());
      unsigned    digits = 64;
      while (digits < std::numeric_limits<result_type>::digits)
      {
         result <<= 64;
         result |= m_gen64();
         digits += 64;
      }
      return result;
   }

 private:
   kiss_rand m_gen64;
};

template <class UnsignedInteger>
constexpr UnsignedInteger nth_random_value(unsigned count = 0)
{
   std::uint64_t                             date_hash = string_to_hash(__DATE__);
   std::uint64_t                             time_hash = string_to_hash(__TIME__);
   multiprecision_generator<UnsignedInteger> big_gen(date_hash, time_hash);
   for (unsigned i = 0; i < count; ++i)
      big_gen();
   return big_gen();
}

int main()
{
   using namespace boost::multiprecision;

//[random_constexpr_cppint
   constexpr uint1024_t rand = nth_random_value<uint1024_t>(1000);
   std::cout << std::hex << rand << std::endl;
//] [/random_constexpr_cppint]
   return 0;
}
