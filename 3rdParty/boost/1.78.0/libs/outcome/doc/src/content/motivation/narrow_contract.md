+++
title = "Narrow contracts"
description = "Describes narrow-contract functions that do not work for all input values, and advantage of using them."
weight = 60
+++

A program's thread of execution can enter a "disappointing" state for two reasons:

* due to disappointing situation in the environment (operating system, external input),
  or
* due to a bug in the program.

The key to handling these disappointments correctly is to identify to which
category they belong, and use the tools adequate for a given category. In this
tutorial when we say "error" or "failure" we only refer to the first category.
A bug is not an error.

A bug is when a program is something else than what it is supposed to be. The
correct action in that case is to change the program so that it is exactly what
it is supposed to be. Unfortunately, sometimes the symptoms of a bug are only
detected when the system is running and at this point no code changes are possible.

In contrast, a failure is when a correct function in a correct program reflects
some disappointing behavior in the environment. The correct action in that case
is for the program to take a control path different than usual, which will likely
cancel some operations and will likely result in different communication with the
outside world.

Symptoms of bugs can sometimes be detected during compilation or static program
analysis or at run-time when observing certain values of objects that are declared
never to be valid at certain points. One classical example is passing a null pointer
to functions that expect a pointer to a valid object:

```c++
int f(int * pi) // expects: pi != nullptr
{
  return *pi + 1;
}
```

Passing a null pointer where it is not expected is so common a bug that tools
are very good at finding them. For instance, static analyzers will usually detect
it without even executing your code. Similarly, tools like undefined behavior
sanitizers will compile a code as the one above so that a safety check is performed
to check if the pointer is null, and an error message will be logged and program
optionally terminated.

More, compilers can perform optimizations based on undefined behavior caused by
dereferencing a null pointer. In the following code:

```c++
pair<int, int> g(int * pi) // expects: pi != nullptr
{
  int i = *pi + 1;
  int j = (pi == nullptr) ? 1 : 0;
  return {i, j};
}
```

The compiler can see that if `pi` is null, the program would have undefined
behavior. Since undefined behavior is required by the C++ standard to never
be the programmer's intention, the compiler
assumes that apparently this function is never called with `pi == nullptr`. If so,
`j` is always `0` and the code can be transformed to a faster one:

```c++
pair<int, int> g(int * pi) // expects: pi != nullptr
{
  int i = *pi + 1;
  int j = 0;
  return {i, j};
}
```

Functions like the one above that declare that certain values of input parameters
must not be passed to them are said to have a *narrow contract*.

Compilers give you non-standard tools to tell them about narrow contracts, so
that they can detect it and make use of it the same way as they are detecting
invalid null pointers. For instance, if a function in your library takes an `int`
and declares that the value of this `int` must never be negative. You can use
`__builtin_trap()` available in GCC and clang:

```c++
void h(int i) // expects: i >= 0
{
  if (i < 0) __builtin_trap();

  // normal program logic follows ...
}
```

This instruction when hit, causes the program to exit abnormally, which means:

* a debugger can be launched,
* static analyzer can warn you if it can detect a program flow that reaches this
  point,
* UB-sanitizer can log error message when it hits it.

Another tool you could use is `__builtin_unreachable()`, also available in GCC
and clang:

```c++
void h(int i) // expects: i >= 0
{
  if (i < 0) __builtin_unreachable();

  // normal program logic follows ...
}
```

This gives a hint to the tools: the programmer guarantees that the program flow
will never reach to the point of executing it. In other words, it is undefined
behavior if control reaches this point. Compiler and other tools can take this
for granted. This way they can deduce that expression `i < 0` will never be true,
and they can further use this assumption to issue warnings or to optimize the code.
UB-sanitizers can use it to inject a log message and terminate if this point is
nonetheless reached.

Allowing for some input values to be invalid works similarly to cyclic redundancy
checks. It allows for the possibility to observe the symptoms of the bugs (not
the bugs themselves), and if the symptom is revealed the hunt for the bug can start.
This is not only tools that can now easily detect symptoms of bugs, but also
humans during the code review. A reviewer can now say, "hey, function `h()` is
expecting a non-negative value, but this `i` is actually `-1`; maybe you wanted
to pass `j` instead?".
