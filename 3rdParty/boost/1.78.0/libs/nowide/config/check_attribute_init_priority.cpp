//  Copyright (c) 2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at http://www.boost.org/LICENSE.txt)
//

class Foo
{};

Foo foo __attribute__((init_priority(101)));

int main()
{
    return 0;
}
