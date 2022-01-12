+++
title = "C Macro API Reference"
weight = 50
+++

The C macro API header `<boost/outcome/experimental/result.h>` consists of these macros:

<dl>
<dt><code>BOOST_OUTCOME_C_DECLARE_RESULT(ident, T, E)</code>
<dd>Declares to C a <code>basic_result<T, E></code> type uniquely
identified by <code>ident</code>. <code>T</code> is available at the
member variable <code>.value</code>, and <code>E</code> is available
at the member variable <code>.error</code>.

<dt><code>BOOST_OUTCOME_C_RESULT(ident)</code>
<dd>A reference to a previously declared <code>result</code> type with
unique <code>ident</code>.

<dt><code>BOOST_OUTCOME_C_RESULT_HAS_VALUE(r)</code>
<dd>Evaluates to 1 (true) if the input <code>result</code> has a value.

<dt><code>BOOST_OUTCOME_C_RESULT_HAS_ERROR(r)</code>
<dd>Evaluates to 1 (true) if the input <code>result</code> has an error.

<dt><code>BOOST_OUTCOME_C_RESULT_ERROR_IS_ERRNO(r)</code>
<dd>Evaluates to 1 (true) if the input <code>result</code>'s error value
is a code in the POSIX <code>errno</code> domain.
</dl>

The above let you work, somewhat awkwardly, with any C-compatible
`basic_result<T, E>`. `basic_result<T, E>` is trivially copyable and
standard layout if its `T` and `E` are both so, and it has the C layout:

```c++
struct cxx_result_##ident
{
  union
  {
    T value;
    E error;
  };
  unsigned flags;
};
```

### `<system_error2>` support

Because erased status codes are not trivially copyable and
therefore do not have union based storage, we have separate C macros
for results whose `E` is an erased status code:

<dl>
<dt><code>BOOST_OUTCOME_C_DECLARE_STATUS_CODE(ident, value_type)</code>
<dd>Declares to C a status code type with domain <code>value_type</code>
available at the member variable <code>.value</code>. The <code>ident</code>
must be any identifier fragment unique in this translation unit. It is
used to uniquely identify this status code type in other macros.

<dt><code>BOOST_OUTCOME_C_STATUS_CODE(ident)</code>
<dd>A reference to a previously declared status code type with unique
<code>ident</code>.

<dt><code>BOOST_OUTCOME_C_DECLARE_RESULT_STATUS_CODE(ident, T, E)</code>
<dd>Declares to C a <code>basic_result<T, E></code> type uniquely
identified by <code>ident</code>. <code>T</code> is available at the
member variable <code>.value</code>, and <code>E</code> is available
at the member variable <code>.error</code>.

<dt><code>BOOST_OUTCOME_C_RESULT_STATUS_CODE(ident)</code>
<dd>A reference to a previously declared <code>result</code> type with
unique <code>ident</code>.
</dl>

There is a high likelihood that C++ functions regularly called by C
code will return their failures either in erased `system_code`
or in `posix_code` (i.e. `errno` code domain). Via querying the
returned value using `BOOST_OUTCOME_C_RESULT_ERROR_IS_ERRNO(r)`, one can determine
if the returned code is in the `errno` code domain, and thus can be
fed to `strerror()` and so on. Therefore there are
convenience macro APIs for those particular use cases.

<dl>
<dt><code>BOOST_OUTCOME_C_DECLARE_RESULT_ERRNO(ident, T)</code>
<dd>Declares to C a <code>basic_result&lt;T, posix_code&gt;</code>
type uniquely identified by <code>ident</code>.

<dt><code>BOOST_OUTCOME_C_RESULT_ERRNO(ident)</code>
<dd>A reference to a previously declared <code>basic_result&lt;T, posix_code&gt;</code>
type with unique <code>ident</code>.

<dt><code>BOOST_OUTCOME_C_DECLARE_RESULT_SYSTEM(ident, T)</code>
<dd>Declares to C a <code>basic_result&lt;T, system_code&gt;</code>
type uniquely identified by <code>ident</code>.

<dt><code>BOOST_OUTCOME_C_RESULT_SYSTEM(ident)</code>
<dd>A reference to a previously declared <code>basic_result&lt;T, system_code&gt;</code>
type with unique <code>ident</code>.
</dl>
