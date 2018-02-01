#include "tz.h"
#include <type_traits>

int
main()
{
    using namespace std;
    using namespace date;
    static_assert( is_nothrow_destructible<time_zone>{}, "");
    static_assert(!is_default_constructible<time_zone>{}, "");
    static_assert(!is_copy_constructible<time_zone>{}, "");
    static_assert(!is_copy_assignable<time_zone>{}, "");
    static_assert( is_nothrow_move_constructible<time_zone>{}, "");
    static_assert( is_nothrow_move_assignable<time_zone>{}, "");
}
