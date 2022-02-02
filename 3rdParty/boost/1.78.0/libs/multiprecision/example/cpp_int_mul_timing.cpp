///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019.                        //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <iterator>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

class random_pcg32_fast_base
{
protected:
  using itype = std::uint64_t;

  static constexpr bool is_mcg = false;

public:
  virtual ~random_pcg32_fast_base() = default;

protected:
  explicit random_pcg32_fast_base(const itype = itype()) { }

  random_pcg32_fast_base(const random_pcg32_fast_base&) = default;

  random_pcg32_fast_base& operator=(const random_pcg32_fast_base&) = default;

  template<typename ArithmeticType>
  static ArithmeticType rotr(const ArithmeticType& value_being_shifted,
                             const std::size_t     bits_to_shift)
  {
    const std::size_t left_shift_amount =
      std::numeric_limits<ArithmeticType>::digits - bits_to_shift;

    const ArithmeticType part1 = ((bits_to_shift > 0U) ? ArithmeticType(value_being_shifted >> bits_to_shift)     : value_being_shifted);
    const ArithmeticType part2 = ((bits_to_shift > 0U) ? ArithmeticType(value_being_shifted << left_shift_amount) : 0U);

    return ArithmeticType(part1 | part2);
  }

  template<typename xtype,
           typename itype>
  struct xsh_rr_mixin
  {
    static xtype output(const itype internal_value)
    {
      using bitcount_t = std::size_t;

      constexpr bitcount_t bits         = bitcount_t(sizeof(itype) * 8U);
      constexpr bitcount_t xtypebits    = bitcount_t(sizeof(xtype) * 8U);
      constexpr bitcount_t sparebits    = bits - xtypebits;
      constexpr bitcount_t wantedopbits =   ((xtypebits >= 128U) ? 7U
                                          : ((xtypebits >=  64U) ? 6U
                                          : ((xtypebits >=  32U) ? 5U
                                          : ((xtypebits >=  16U) ? 4U
                                          :                        3U))));

      constexpr bitcount_t opbits       = ((sparebits >= wantedopbits) ? wantedopbits : sparebits);
      constexpr bitcount_t amplifier    = wantedopbits - opbits;
      constexpr bitcount_t mask         = (1ULL << opbits) - 1U;
      constexpr bitcount_t topspare     = opbits;
      constexpr bitcount_t bottomspare  = sparebits - topspare;
      constexpr bitcount_t xshift       = (topspare + xtypebits) / 2U;

      const bitcount_t rot =
        ((opbits != 0U) ? (bitcount_t(internal_value >> (bits - opbits)) & mask)
                        : 0U);

      const bitcount_t amprot = (rot << amplifier) & mask;

      const itype internal_value_xor = internal_value ^ itype(internal_value >> xshift);

      const xtype result = rotr(xtype(internal_value_xor >> bottomspare), amprot);

      return result;
    }
  };
};

class random_pcg32_fast : public random_pcg32_fast_base
{
private:
  static constexpr itype default_multiplier = static_cast<itype>(6364136223846793005ULL);
  static constexpr itype default_increment  = static_cast<itype>(1442695040888963407ULL);

public:
  using result_type = std::uint32_t;

  static constexpr itype default_seed = static_cast<itype>(0xCAFEF00DD15EA5E5ULL);

  explicit random_pcg32_fast(const itype state = default_seed)
    : random_pcg32_fast_base(state),
      my_inc  (default_increment),
      my_state(is_mcg ? state | itype(3U) : bump(state + increment())) { }

  random_pcg32_fast(const random_pcg32_fast& other)
    : random_pcg32_fast_base(other),
      my_inc  (other.my_inc),
      my_state(other.my_state) { }

  virtual ~random_pcg32_fast() = default;

  random_pcg32_fast& operator=(const random_pcg32_fast& other)
  {
    static_cast<void>(random_pcg32_fast_base::operator=(other));

    if(this != &other)
    {
      my_inc   = other.my_inc;
      my_state = other.my_state;
    }

    return *this;
  }

  void seed(const itype state = default_seed)
  {
    my_inc = default_increment;

    my_state = (is_mcg ? state | itype(3U) : bump(state + increment()));
  }

  result_type operator()()
  {
    const result_type value =
      xsh_rr_mixin<result_type, itype>::output(base_generate0());

    return value;
  }

private:
  itype my_inc;
  itype my_state;

  itype multiplier() const
  {
    return default_multiplier;
  }

  itype increment() const
  {
    return default_increment;
  }

  itype bump(const itype state)
  {
    return itype(state * multiplier()) + increment();
  }

  itype base_generate0()
  {
    const itype old_state = my_state;

    my_state = bump(my_state);

    return old_state;
  }
};


template<typename UnsignedIntegralIteratorType,
         typename RandomEngineType>
void get_random_big_uint(RandomEngineType& rng, UnsignedIntegralIteratorType it_out)
{
  using local_random_value_type = typename RandomEngineType::result_type;

  using local_uint_type = typename std::iterator_traits<UnsignedIntegralIteratorType>::value_type;

  constexpr std::size_t digits_of_uint___type = static_cast<std::size_t>(std::numeric_limits<local_uint_type>::digits);
  constexpr std::size_t digits_of_random_type = static_cast<std::size_t>(std::numeric_limits<local_random_value_type>::digits);

  local_random_value_type next_random = rng();

  *it_out = next_random;

  for(std::size_t i = digits_of_random_type; i < digits_of_uint___type; i += digits_of_random_type)
  {
    (*it_out) <<= digits_of_random_type;

    next_random = rng();

    (*it_out) |= next_random;
  }
}

using big_uint_backend_type =
  boost::multiprecision::cpp_int_backend<8192UL << 1U,
                                         8192UL << 1U,
                                         boost::multiprecision::unsigned_magnitude>;

using big_uint_type = boost::multiprecision::number<big_uint_backend_type>;

namespace local
{
  std::vector<big_uint_type> a(1024U);
  std::vector<big_uint_type> b(a.size());
}

int main()
{
  random_pcg32_fast rng;

  rng.seed(std::clock());

  for(auto i = 0U; i < local::a.size(); ++i)
  {
    get_random_big_uint(rng, local::a.begin() + i);
    get_random_big_uint(rng, local::b.begin() + i);
  }

  std::size_t count = 0U;

  float total_time = 0.0F;

  const std::clock_t start = std::clock();

  do
  {
    const std::size_t index = count % local::a.size();

    local::a[index] * local::b[index];

    ++count;
  }
  while((total_time = (float(std::clock() - start) / float(CLOCKS_PER_SEC))) < 10.0F);

  // Boost.Multiprecision 1.71
  // bits: 16384, kops_per_sec: 4.7
  // bits: 32768, kops_per_sec: 1.2
  // bits: 65536, kops_per_sec: 0.3
  // bits: 131072, kops_per_sec: 0.075
  // bits: 262144, kops_per_sec: 0.019
  // bits: 524288, kops_per_sec: 0.0047

  // Boost.Multiprecision + kara mult
  // bits: 16384, kops_per_sec: 4.8
  // bits: 32768, kops_per_sec: 1.6
  // bits: 65536, kops_per_sec: 0.5
  // bits: 131072, kops_per_sec: 0.15
  // bits: 262144, kops_per_sec: 0.043
  // bits: 524288, kops_per_sec: 0.011

  const float kops_per_sec = (float(count) / total_time) / 1000.0F;

  std::cout << "bits: "
            << std::numeric_limits<big_uint_type>::digits
            << ", kops_per_sec: "
            << kops_per_sec
            << count << std::endl;
}
