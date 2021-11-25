// Copyright 2017 Peter Dimov
// Copyright 2017 Vinnie Falco
// Copyright 2018 Andrzej Krzemienski
//
// Distributed under the Boost Software License, Version 1.0.
//
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>

#if defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

int main()
{
}

#else

#include <boost/optional.hpp>
#include <utility>

class basic_multi_buffer;

class const_buffers_type // a similar declaration in boost.beast had problem
{                        // with boost opitonal
    basic_multi_buffer const* b_;

    friend class basic_multi_buffer;

    explicit
    const_buffers_type(basic_multi_buffer const& b);

public:
    const_buffers_type() = delete;
    const_buffers_type(const_buffers_type const&) = default;
    const_buffers_type& operator=(const_buffers_type const&) = default;
};

void test_beast_example()
{
  // test if it even compiles
  boost::optional< std::pair<const_buffers_type, int> > opt, opt2;
  opt = opt2;
  (void)opt;
}

struct NotDefaultConstructible // minimal class exposing the problem
{
  NotDefaultConstructible() = delete;
};

void test_assign_for_non_default_constructible()
{
  // test if it even compiles
  boost::optional<NotDefaultConstructible> opt, opt2;
  opt = opt2;
  (void)opt;
}

int main()
{
  test_beast_example();
  test_assign_for_non_default_constructible();
}

#endif
