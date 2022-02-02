+++
title = "Mapping the HTTP library into the Application `1/2`"
weight = 30
+++

Firstly, remember that we are the application writer who has the problem of
integrating three third party libraries into our application's Outcome-based
failure handling mechanism. We cannot modify those third party library
sources; we must be *non-intrusive*.

We start by dealing with the HTTP library. We will integrate this
into our application by wrapping up `httplib::failure` into a custom
STL exception type. We then type erase it into an `exception_ptr`
instance. Please note that this code is exclusively defined in the `app` namespace:

{{% snippet "finale.cpp" "app_map_httplib1" %}}

Most of the complexity in this code fragment is driven by the need to create
some sort of descriptive string for `std::runtime_error`
so its `.what()` returns a useful summary of the original failure. This
is the main purpose of the `app::make_httplib_exception()` function.

(Note that if you have Reflection in your C++ compiler, it may be possible to script
the conversion of enum values to string representations)

The only real thing to note about `app::httplib_error` is that it squirrels away
the original `httplib::failure` in case that is ever needed. 
