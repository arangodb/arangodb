+++
title = "Approximate map between error code designs"
weight = 10
+++

Much of the design of Boost.System (which went on to become `<system_error>`)
has been retained in proposed `<system_error2>`, so an approximate map between
`<system_error2>` and `<system_error>` and Boost.System can be given:

<table width="100%" cellpadding="5" border="1">
<colgroup>
 <col width="25%">
 <col width="25%">
 <col width="50%">
</colgroup>
<tr><th>C++ 17 <tt>&lt;system_error&gt;</tt><th>Boost.System<th>Proposed <tt>&lt;system_error2&gt;</tt>
<tr>
 <td style="vertical-align: top;"><tt>std::errc</tt>
 <td style="vertical-align: top;"><tt>boost::system::errc</tt>
 <td style="vertical-align: top;"><tt>experimental::errc</tt> (almost identical)
<tr>
 <td style="vertical-align: top;"><tt>std::error_category</tt>
 <td style="vertical-align: top;"><tt>boost::system::error_category</tt>
 <td style="vertical-align: top;"><tt>experimental::status_code_domain</tt>
<tr>
 <td style="vertical-align: top;"><tt>std::generic_category</tt>
 <td style="vertical-align: top;"><tt>boost::system::generic_category</tt>
 <td style="vertical-align: top;"><tt>experimental::generic_code_domain</tt>
<tr>
 <td style="vertical-align: top;"><tt>std::system_category</tt>
 <td style="vertical-align: top;"><tt>boost::system::system_category</tt>
 <td style="vertical-align: top;">One of:<ul>
  <li><tt>experimental::posix_code_domain</tt> (POSIX systems)<p>
  <li><tt>experimental::win32_code_domain</tt> (Microsoft Windows)<p>
  <li><tt>experimental::nt_code_domain</tt> (Microsoft Windows)
 </ul>
 Note that there are more precanned code categories though they require additional header inclusions:
<tt>com_code</tt>, <tt>getaddrinfo_code</tt>.
<tr>
 <td style="vertical-align: top;"><tt>std::error_condition</tt>
 <td style="vertical-align: top;"><tt>boost::system::error_condition</tt>
 <td style="vertical-align: top;"><b>No equivalent</b> (deliberately removed as hindsight proved it to be a design mistake leading to much confusing and hard to audit for correctness code)
<tr>
 <td style="vertical-align: top;"><tt>std::error_code</tt>
 <td style="vertical-align: top;"><tt>boost::system::error_code</tt>
 <td style="vertical-align: top;">One of:<ul>
  <li><tt>experimental::status_code&lt;DomainType&gt;</tt><p>
  <li><tt>const experimental::status_code&lt;void&gt; &amp;</tt><p>
  <li><tt>experimental::status_code&lt;erased&lt;intptr_t&gt;&gt;</tt> (aliased to <tt>experimental::system_code</tt>)<p>
  <li><tt>experimental::errored_status_code&lt;DomainType&gt;</tt><p>
  <li><tt>const experimental::errored_status_code&lt;void&gt; &amp;</tt><p>
  <li><tt>experimental::errored_status_code&lt;erased&lt;intptr_t&gt;&gt;</tt> (aliased to <tt>experimental::error</tt>)
 </ul>
 The difference between status codes and errored status codes is that the latter are guaranteed
 to refer to a failure, whereas the former may refer to a success (including warnings and informationals).
<tr>
 <td style="vertical-align: top;"><tt>std::system_error</tt>
 <td style="vertical-align: top;"><tt>boost::system::system_error</tt>
 <td style="vertical-align: top;">One of:<ul>
  <li><tt>const experimental::status_error&lt;void&gt; &amp;</tt><p>
  <li><tt>experimental::status_error&lt;DomainType&gt;</tt>
 </ul>
</table>

As is obvious from the above, in `<system_error2>` one must be much more specific and accurate
with respect to intent and specification and desired semantics than with `<system_error>`. Much
ambiguity and incorrectness which flies silently in `<system_error>` will
refuse to compile in `<system_error2>`.
