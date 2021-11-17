+++
title = "The Filesystem TS"
description = ""
weight = 10
tags = ["dual-api"]
+++

Something which has long annoyed the purists in the C++ leadership is the problem of dual
overloads in `error_code` capable standard library APIs.

Consider the
[`copy_file()`](http://en.cppreference.com/w/cpp/filesystem/copy_file)
API from the Filesystem TS:

{{% snippet "outcome_payload.cpp" "filesystem_api_problem" %}}

Before Outcome, the common design pattern was to provide throwing and non-throwing overloads
of every API. As you can see above, the throwing API throws a [`filesystem::filesystem_error`](http://en.cppreference.com/w/cpp/filesystem/filesystem_error)
exception type which carries additional information, specifically two paths. These paths may
refer to the files which were the source of any failure. However the non-throwing overload
does **not** provide this additional information, which can make it more annoying to use the
non-throwing overload sometimes.

What if we could replace these two overloads of every API in the Filesystem TS with a single API,
and additionally have the non-throwing edition return the exact same additional information
as the throwing edition?
