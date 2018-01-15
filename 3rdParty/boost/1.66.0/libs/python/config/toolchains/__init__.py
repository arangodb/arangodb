#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import traceback

def append_feature_flag(env, **kw):
    stack = traceback.extract_stack(limit = 3)
    feature = stack[0][2].upper()
    for (key, val) in kw.items():
        feature_var = feature + "_" + key
        env.AppendUnique(**{ key : "$" + feature_var })
        env[feature_var] = val

