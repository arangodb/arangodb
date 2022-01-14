# Copyright (C) 2003 Rational Discovery LLC.  Distributed under the Boost
# Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy
# at http://www.boost.org/LICENSE_1_0.txt)

print("running...")

try:
    import module_init_exception_ext
except RuntimeError as e:
    print(e)

print("Done.")
