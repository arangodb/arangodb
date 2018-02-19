// -----------------------------------------------------------
//              Copyright (c) 2001 Jeremy Siek
//           Copyright (c) 2003-2006 Gennaro Prota
//             Copyright (c) 2014 Ahmed Charles
//          Copyright (c) 2014 Riccardo Marcangelo
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#include <assert.h>
#include "bitset_test.hpp"
#include "boost/dynamic_bitset/dynamic_bitset.hpp"
#include "boost/limits.hpp"
#include "boost/config.hpp"

template <typename Block>
void run_test_cases( BOOST_EXPLICIT_TEMPLATE_TYPE(Block) )
{
  // a bunch of typedefs which will be handy later on
  typedef boost::dynamic_bitset<Block> bitset_type;
  typedef bitset_test<bitset_type> Tests;
  // typedef typename bitset_type::size_type size_type; // unusable with Borland 5.5.1

  std::string long_string = get_long_string();
  std::size_t ul_width = std::numeric_limits<unsigned long>::digits;

  //=====================================================================
  // Test b.empty()
  {
    bitset_type b;
    Tests::empty(b);
  }
  {
    bitset_type b(1, 1ul);
    Tests::empty(b);
  }
  {
    bitset_type b(bitset_type::bits_per_block
                  + bitset_type::bits_per_block/2, 15ul);
    Tests::empty(b);
  }
  //=====================================================================
  // Test b.to_long()
  {
    boost::dynamic_bitset<Block> b;
    Tests::to_ulong(b);
  }
  {
    boost::dynamic_bitset<Block> b(std::string("1"));
    Tests::to_ulong(b);
  }
  {
    boost::dynamic_bitset<Block> b(bitset_type::bits_per_block,
                                   static_cast<unsigned long>(-1));
    Tests::to_ulong(b);
  }
  {
    std::string str(ul_width - 1, '1');
    boost::dynamic_bitset<Block> b(str);
    Tests::to_ulong(b);
  }
  {
    std::string ul_str(ul_width, '1');
    boost::dynamic_bitset<Block> b(ul_str);
    Tests::to_ulong(b);
  }
  { // case overflow
    boost::dynamic_bitset<Block> b(long_string);
    Tests::to_ulong(b);
  }
  //=====================================================================
  // Test to_string(b, str)
  {
    boost::dynamic_bitset<Block> b;
    Tests::to_string(b);
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::to_string(b);
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::to_string(b);
  }
  //=====================================================================
  // Test b.count()
  {
    boost::dynamic_bitset<Block> b;
    Tests::count(b);
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::count(b);
  }
  {
    boost::dynamic_bitset<Block> b(std::string("1"));
    Tests::count(b);
  }
  {
    boost::dynamic_bitset<Block> b(8, 255ul);
    Tests::count(b);
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::count(b);
  }
  //=====================================================================
  // Test b.size()
  {
    boost::dynamic_bitset<Block> b;
    Tests::size(b);
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::size(b);
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::size(b);
  }
  //=====================================================================
  // Test b.capacity()
  {
    boost::dynamic_bitset<Block> b;
    Tests::capacity_test_one(b);
  }
  {
    boost::dynamic_bitset<Block> b(100);
    Tests::capacity_test_two(b);
  }
  //=====================================================================
  // Test b.reserve()
  {
    boost::dynamic_bitset<Block> b;
    Tests::reserve_test_one(b);
  }
  {
    boost::dynamic_bitset<Block> b(100);
    Tests::reserve_test_two(b);
  }
  //=====================================================================
  // Test b.shrink_to_fit()
  {
    boost::dynamic_bitset<Block> b;
    Tests::shrink_to_fit_test_one(b);
  }
  {
    boost::dynamic_bitset<Block> b(100);
    Tests::shrink_to_fit_test_two(b);
  }
  //=====================================================================
  // Test b.all()
  {
    boost::dynamic_bitset<Block> b;
    Tests::all(b);
    Tests::all(~b);
    Tests::all(b.set());
    Tests::all(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::all(b);
    Tests::all(~b);
    Tests::all(b.set());
    Tests::all(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::all(b);
    Tests::all(~b);
    Tests::all(b.set());
    Tests::all(b.reset());
  }
  //=====================================================================
  // Test b.any()
  {
    boost::dynamic_bitset<Block> b;
    Tests::any(b);
    Tests::any(~b);
    Tests::any(b.set());
    Tests::any(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::any(b);
    Tests::any(~b);
    Tests::any(b.set());
    Tests::any(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::any(b);
    Tests::any(~b);
    Tests::any(b.set());
    Tests::any(b.reset());
  }
  //=====================================================================
  // Test b.none()
  {
    boost::dynamic_bitset<Block> b;
    Tests::none(b);
    Tests::none(~b);
    Tests::none(b.set());
    Tests::none(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::none(b);
    Tests::none(~b);
    Tests::none(b.set());
    Tests::none(b.reset());
  }
  {
    boost::dynamic_bitset<Block> b(long_string);
    Tests::none(b);
    Tests::none(~b);
    Tests::none(b.set());
    Tests::none(b.reset());
  }
  //=====================================================================
  // Test a.is_subset_of(b)
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::subset(a, b);
  }
  //=====================================================================
  // Test a.is_proper_subset_of(b)
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::proper_subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::proper_subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::proper_subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::proper_subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::proper_subset(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::proper_subset(a, b);
  }
  //=====================================================================
  // Test intersects
  {
    bitset_type a; // empty
    bitset_type b;
    Tests::intersects(a, b);
  }
  {
    bitset_type a;
    bitset_type b(5, 8ul);
    Tests::intersects(a, b);
  }
  {
    bitset_type a(8, 0ul);
    bitset_type b(15, 0ul);
    b[9] = 1;
    Tests::intersects(a, b);
  }
  {
    bitset_type a(15, 0ul);
    bitset_type b(22, 0ul);
    a[14] = b[14] = 1;
    Tests::intersects(a, b);
  }
  //=====================================================================
  // Test find_first
  {
      // empty bitset
      bitset_type b;
      Tests::find_first(b);
  }
  {
      // bitset of size 1
      bitset_type b(1, 1ul);
      Tests::find_first(b);
  }
  {
      // all-0s bitset
      bitset_type b(4 * bitset_type::bits_per_block, 0ul);
      Tests::find_first(b);
  }
  {
      // first bit on
      bitset_type b(1, 1ul);
      Tests::find_first(b);
  }
  {
      // last bit on
      bitset_type b(4 * bitset_type::bits_per_block - 1, 0ul);
      b.set(b.size() - 1);
      Tests::find_first(b);
  }
  //=====================================================================
  // Test find_next
  {
      // empty bitset
      bitset_type b;

      // check
      Tests::find_next(b, 0);
      Tests::find_next(b, 1);
      Tests::find_next(b, 200);
      Tests::find_next(b, b.npos);
  }
  {
      // bitset of size 1 (find_next can never find)
      bitset_type b(1, 1ul);

      // check
      Tests::find_next(b, 0);
      Tests::find_next(b, 1);
      Tests::find_next(b, 200);
      Tests::find_next(b, b.npos);
  }
  {
      // all-1s bitset
      bitset_type b(16 * bitset_type::bits_per_block);
      b.set();

      // check
      const typename bitset_type::size_type larger_than_size = 5 + b.size();
      for(typename bitset_type::size_type i = 0; i <= larger_than_size; ++i) {
          Tests::find_next(b, i);
      }
      Tests::find_next(b, b.npos);
  }
  {
      // a bitset with 1s at block boundary only
      const int num_blocks = 32;
      const int block_width = bitset_type::bits_per_block;

      bitset_type b(num_blocks * block_width);
      typename bitset_type::size_type i = block_width - 1;
      for ( ; i < b.size(); i += block_width) {

        b.set(i);
        typename bitset_type::size_type first_in_block = i - (block_width - 1);
        b.set(first_in_block);
      }

      // check
      const typename bitset_type::size_type larger_than_size = 5 + b.size();
      for (i = 0; i <= larger_than_size; ++i) {
          Tests::find_next(b, i);
      }
      Tests::find_next(b, b.npos);

  }
  {
      // bitset with alternate 1s and 0s
      const typename bitset_type::size_type sz = 1000;
      bitset_type b(sz);

      typename bitset_type::size_type i = 0;
      for ( ; i < sz; ++i) {
        b[i] = (i%2 == 0);
      }

      // check
      const typename bitset_type::size_type larger_than_size = 5 + b.size();
      for (i = 0; i <= larger_than_size; ++i) {
          Tests::find_next(b, i);
      }
      Tests::find_next(b, b.npos);

  }
  //=====================================================================
  // Test operator==
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_equal(a, b);
  }
  //=====================================================================
  // Test operator!=
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_not_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_not_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_not_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_not_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_not_equal(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_not_equal(a, b);
  }
  //=====================================================================
  // Test operator<
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("10")), b(std::string("11"));
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("101")), b(std::string("11"));
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("10")), b(std::string("111"));
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_less_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_less_than(a, b);
  }
  // check for consistency with ulong behaviour when the sizes are equal
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 5ul);
    assert(a < b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 4ul);
    assert(!(a < b));
  }
  {
    boost::dynamic_bitset<Block> a(3, 5ul), b(3, 4ul);
    assert(!(a < b));
  }
  // when the sizes are not equal lexicographic compare does not necessarily correspond to ulong behavior
  {
    boost::dynamic_bitset<Block> a(4, 4ul), b(3, 5ul);
    assert(a < b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(4, 5ul);
    assert(!(a < b));
  }
  {
    boost::dynamic_bitset<Block> a(4, 4ul), b(3, 4ul);
    assert(a < b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(4, 4ul);
    assert(!(a < b));
  }
  {
    boost::dynamic_bitset<Block> a(4, 5ul), b(3, 4ul);
    assert(a < b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 5ul), b(4, 4ul);
    assert(!(a < b));
  }
  //=====================================================================
  // Test operator<=
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_less_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_less_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_less_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_less_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_less_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_less_than_eq(a, b);
  }
  // check for consistency with ulong behaviour
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 5ul);
    assert(a <= b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 4ul);
    assert(a <= b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 5ul), b(3, 4ul);
    assert(!(a <= b));
  }
  //=====================================================================
  // Test operator>
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_greater_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_greater_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_greater_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_greater_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_greater_than(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_greater_than(a, b);
  }
  // check for consistency with ulong behaviour
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 5ul);
    assert(!(a > b));
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 4ul);
    assert(!(a > b));
  }
  {
    boost::dynamic_bitset<Block> a(3, 5ul), b(3, 4ul);
    assert(a > b);
  }
  //=====================================================================
  // Test operator<=
  {
    boost::dynamic_bitset<Block> a, b;
    Tests::operator_greater_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("0")), b(std::string("0"));
    Tests::operator_greater_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(std::string("1")), b(std::string("1"));
    Tests::operator_greater_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    Tests::operator_greater_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    a[long_string.size()/2].flip();
    Tests::operator_greater_than_eq(a, b);
  }
  {
    boost::dynamic_bitset<Block> a(long_string), b(long_string);
    b[long_string.size()/2].flip();
    Tests::operator_greater_than_eq(a, b);
  }
  // check for consistency with ulong behaviour
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 5ul);
    assert(!(a >= b));
  }
  {
    boost::dynamic_bitset<Block> a(3, 4ul), b(3, 4ul);
    assert(a >= b);
  }
  {
    boost::dynamic_bitset<Block> a(3, 5ul), b(3, 4ul);
    assert(a >= b);
  }
  //=====================================================================
  // Test b.test(pos)
  { // case pos >= b.size()
    boost::dynamic_bitset<Block> b;
    Tests::test_bit(b, 0);
  }
  { // case pos < b.size()
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::test_bit(b, 0);
  }
  { // case pos == b.size() / 2
    boost::dynamic_bitset<Block> b(long_string);
    Tests::test_bit(b, long_string.size()/2);
  }
  //=====================================================================
  // Test b.test_set(pos)
  { // case pos >= b.size()
    boost::dynamic_bitset<Block> b;
    Tests::test_set_bit(b, 0, true);
    Tests::test_set_bit(b, 0, false);
  }
  { // case pos < b.size()
    boost::dynamic_bitset<Block> b(std::string("0"));
    Tests::test_set_bit(b, 0, true);
    Tests::test_set_bit(b, 0, false);
  }
  { // case pos == b.size() / 2
    boost::dynamic_bitset<Block> b(long_string);
    Tests::test_set_bit(b, long_string.size() / 2, true);
    Tests::test_set_bit(b, long_string.size() / 2, false);
  }
  //=====================================================================
  // Test b << pos
  { // case pos == 0
    std::size_t pos = 0;
    boost::dynamic_bitset<Block> b(std::string("1010"));
    Tests::operator_shift_left(b, pos);
  }
  { // case pos == size()/2
    std::size_t pos = long_string.size() / 2;
    boost::dynamic_bitset<Block> b(long_string);
    Tests::operator_shift_left(b, pos);
  }
  { // case pos >= n
    std::size_t pos = long_string.size();
    boost::dynamic_bitset<Block> b(long_string);
    Tests::operator_shift_left(b, pos);
  }
  //=====================================================================
  // Test b >> pos
  { // case pos == 0
    std::size_t pos = 0;
    boost::dynamic_bitset<Block> b(std::string("1010"));
    Tests::operator_shift_right(b, pos);
  }
  { // case pos == size()/2
    std::size_t pos = long_string.size() / 2;
    boost::dynamic_bitset<Block> b(long_string);
    Tests::operator_shift_right(b, pos);
  }
  { // case pos >= n
    std::size_t pos = long_string.size();
    boost::dynamic_bitset<Block> b(long_string);
    Tests::operator_shift_right(b, pos);
  }
  //=====================================================================
  // Test a & b
  {
    boost::dynamic_bitset<Block> lhs, rhs;
    Tests::operator_and(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(std::string("1")), rhs(std::string("0"));
    Tests::operator_and(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 0), rhs(long_string);
    Tests::operator_and(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 1), rhs(long_string);
    Tests::operator_and(lhs, rhs);
  }
  //=====================================================================
  // Test a | b
  {
    boost::dynamic_bitset<Block> lhs, rhs;
    Tests::operator_or(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(std::string("1")), rhs(std::string("0"));
    Tests::operator_or(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 0), rhs(long_string);
    Tests::operator_or(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 1), rhs(long_string);
    Tests::operator_or(lhs, rhs);
  }
  //=====================================================================
  // Test a^b
  {
    boost::dynamic_bitset<Block> lhs, rhs;
    Tests::operator_xor(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(std::string("1")), rhs(std::string("0"));
    Tests::operator_xor(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 0), rhs(long_string);
    Tests::operator_xor(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 1), rhs(long_string);
    Tests::operator_xor(lhs, rhs);
  }
  //=====================================================================
  // Test a-b
  {
    boost::dynamic_bitset<Block> lhs, rhs;
    Tests::operator_sub(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(std::string("1")), rhs(std::string("0"));
    Tests::operator_sub(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 0), rhs(long_string);
    Tests::operator_sub(lhs, rhs);
  }
  {
    boost::dynamic_bitset<Block> lhs(long_string.size(), 1), rhs(long_string);
    Tests::operator_sub(lhs, rhs);
  }
}


int
test_main(int, char*[])
{
  run_test_cases<unsigned char>();
  run_test_cases<unsigned short>();
  run_test_cases<unsigned int>();
  run_test_cases<unsigned long>();
# ifdef BOOST_HAS_LONG_LONG
  run_test_cases< ::boost::ulong_long_type>();
# endif

  return 0;
}
