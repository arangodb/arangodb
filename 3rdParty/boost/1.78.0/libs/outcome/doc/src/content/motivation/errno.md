+++
title = "errno"
description = "errno with their good and bad sides."
weight = 20
+++


The idiom of returning, upon failure, a special value and storing an error code
(an `int`) inside a global (or thread-local) object `errno` is inherited from C,
and used in its Standard Library:

```c++
int readValue(const char * filename)
{  
  FILE* f = fopen(filename, "r");
  if (f == NULL)
    return 0; // special value indicating failure
              // keep errno value set by fopen()

  int i;
  int r = fscanf(f, "%d", &i);
  if (r == 0 || r == EOF) { // special values: i not read
    errno = ENODATA;        // choose error value to return
    return 0;

  fclose(f);
  errno = 0;  // clear error info (success)
  return i;
}
```

One advantage (to some, and a disadvantage to others) of this technique is that it
uses familiar control statements (`if` and `return`) to indicate all execution
paths that handle failures. When we read this code we know when and under what
conditions it can exit without producing the expected result.


### Downsides


Because on failure, as well as success, we write into a global (or thread-local)
object, our functions are not *pure*: they have *side effects*. This means many
useful compiler optimizations (like common subexpression elimination) cannot be
applied. This shows that it is not only C++ that chooses suboptimal solutions
for reporting failures.

Whatever type we return, we always need a special value to spare, which is
sometimes troublesome. In the above example, if the successfully read value of
`i` is `0`, and we return it, our callers will think it is a failure even though
it is not.

Error propagation using `if` statements and early `return`s is manual. We can easily
forget to check for the failure, and incorrectly let the subsequent operations
execute, potentially causing damage to the program state.

Upon nearly each function call layer we may have to change error code value
so that it reflects the error condition adequate to the current layer. If we
do so, the original error code is gone.
