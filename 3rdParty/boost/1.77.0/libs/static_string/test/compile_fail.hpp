#include <fstream>
#include <streambuf>

namespace boost {
namespace static_strings {

static_assert(std::is_base_of<
  detail::static_string_base<0, char, std::char_traits<char>>,
              static_string<0>>::value,
              "the zero size optimization shall be used for N = 0");

static_assert(std::is_base_of<
  detail::static_string_base<(std::numeric_limits<char>::max)() + 1, char, std::char_traits<char>>,
              static_string<(std::numeric_limits<char>::max)() + 1>>::value,
              "the minimum size type optimization shall be used for N > 0");

static_assert(!detail::is_input_iterator<int>::value, "is_input_iterator is incorrect");
static_assert(!detail::is_input_iterator<double>::value, "is_input_iterator is incorrect");
static_assert(detail::is_input_iterator<int*>::value, "is_input_iterator is incorrect");
static_assert(detail::is_input_iterator<std::istreambuf_iterator<char>>::value, "is_input_iterator is incorrect");

static_assert(!detail::is_forward_iterator<int>::value, "is_forward_iterator is incorrect");
static_assert(!detail::is_forward_iterator<double>::value, "is_forward_iterator is incorrect");
static_assert(detail::is_forward_iterator<int*>::value, "is_forward_iterator is incorrect");
static_assert(!detail::is_forward_iterator<std::istreambuf_iterator<char>>::value, "is_forward_iterator is incorrect");
} // boost
} // static_strings