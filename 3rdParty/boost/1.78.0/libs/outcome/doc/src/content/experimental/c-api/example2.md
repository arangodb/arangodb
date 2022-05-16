+++
title = "Calling it from C"
description = ""
weight = 30
+++

Firstly we need to declare to C our `result` returning C++ function:

{{% snippet "c_api.c" "preamble" %}}

Now let's call the C++ function from C:

{{% snippet "c_api.c" "example" %}}

Running this C program yields:

```
to_string(9) fills buffer with '9' of 1 characters
to_string(99) fills buffer with '99' of 2 characters
to_string(999) fills buffer with '999' of 3 characters
to_string(9999) failed with error code 105 (No buffer space available)
```
