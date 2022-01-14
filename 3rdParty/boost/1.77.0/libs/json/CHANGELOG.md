Boost 1.77.0:

*  Implicit conversion operator from `string` to `std::string_view`.
* `value_to` supports `TupleLike` types.
* `value_to` and `value_from` support `std::array` and similar types.
* `object` deallocates the correct size.
* Fixed crash when constructing `array` from a pair of iterators that form an
  empty range.
* `key_value_pair` allocates with the correct alignment.
* `std::hash` specializations for json types.

Boost 1.76.0:

* Refactored `value_from` implementation; user customizations are now always
  preferred over library-provided overloads.
* Fixed imprecise parsing for some floating point numbers.
* Fixed link errors in standalone mode, when used alongside Boost.
* Fix Boost.Build builds on GCC 4.8.

--------------------------------------------------------------------------------

Boost 1.75.0:

Initial release.
