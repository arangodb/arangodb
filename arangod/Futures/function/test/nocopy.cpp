#include "cxx_function.hpp"

using namespace cxx_function;

struct nc {
nc( nc && ) = default;
nc( nc const & ) = delete;

void operator () () {}
};

static_assert ( std::is_constructible< unique_function< void() >, nc >::value, "" );
static_assert ( ! std::is_constructible< function< void() >, nc >::value, "" );
static_assert ( std::is_constructible< unique_function< void() >, function< void() > >::value, "" );
static_assert ( std::is_constructible< unique_function< void() >, function< void() > & >::value, "" );
static_assert ( ! std::is_copy_constructible< unique_function< void() > >::value, "" );
static_assert ( ! std::is_constructible< function< void() >, unique_function< void() > >::value, "" );

int main() {}

