+++
title = "The File I/O library"
weight = 20
+++

The File I/O library we shall be using is very similar [to the one we saw earlier
in this tutorial](../../payload/copy_file2):

{{% snippet "finale.cpp" "filelib" %}}

This uses the advanced Outcome feature of programming the lazy synthesis of
custom C++ exception throws from a payload carrying `E` type called `failure_info`.
Like the HTTP library, it too template aliases a localised `result` implementation
into its namespace with ADL bridging so Outcome customisation points can be
discovered.

