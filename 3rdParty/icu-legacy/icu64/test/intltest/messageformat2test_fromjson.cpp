// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "messageformat2test.h"

using namespace icu::message2;

/*
  TODO: Tests need to be unified in a single format that
  both ICU4C and ICU4J can use, rather than being embedded in code.

  Tests are included in their current state to give a sense of
  how much test coverage has been achieved. Most of the testing is
  of the parser/serializer; the formatter needs to be tested more
  thoroughly.
*/

/*
Tests reflect the syntax specified in

  https://github.com/unicode-org/message-format-wg/commits/main/spec/message.abnf

as of the following commit from 2023-05-09:
  https://github.com/unicode-org/message-format-wg/commit/194f6efcec5bf396df36a19bd6fa78d1fa2e0867

*/

/*
  Transcribed from https://github.com/messageformat/messageformat/blob/main/packages/mf2-messageformat/src/__fixtures/test-messages.json
https://github.com/messageformat/messageformat/commit/6656c95d66414da29a332a6f5bbb225371f2b9a3

*/
void TestMessageFormat2::jsonTests(IcuTestErrorCode& errorCode) {
    TestCase::Builder testBuilder;
    testBuilder.setName("jsonTests");

    TestCase test = testBuilder.setPattern("hello")
        .setExpected("hello")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|world|}")
                                .setExpected("hello world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {||}")
                                .setExpected("hello ")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {$place}")
                                .setExpected("hello world")
                                .setArgument("place", "world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {$place-.}")
                                .setExpected("hello world")
                                .setArgument("place-.", "world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {$place}")
                                .setExpected("hello {$place}")
                                .clearArguments()
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{$one} and {$two}")
                                .setExpected("1.3 and 4.2")
                                .setExpectSuccess()
                                .setArgument("one", 1.3)
                                .setArgument("two", 4.2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    testBuilder.setArgument("one", "1.3").setArgument("two", "4.2");
    test = testBuilder.build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{$one} et {$two}")
                                .setExpected("1,3 et 4,2")
                                .setLocale(Locale("fr"))
                                .setArgument("one", 1.3)
                                .setArgument("two", 4.2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|4.2| :number}")
                                .setExpected("hello 4.2")
                                .setLocale(Locale("en"))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|foo| :number}")
                                .setExpected("hello {|foo|}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {:number}")
                                .setExpected("hello {:number}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);


    test = testBuilder.setPattern("hello {|4.2| :number minimumFractionDigits=2}")
                                .setExpectSuccess()
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|4.2| :number minimumFractionDigits=|2|}")
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|4.2| :number minimumFractionDigits=$foo}")
                                .setExpected("hello 4.20")
                                .setArgument("foo", (int64_t) 2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {bar} {{bar {$foo}}}")
                                .setExpected("bar bar")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {|bar|} {{bar {$foo}}}")
                                .setExpected("bar bar")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {|bar|} {{bar {$foo}}}")
                                .setExpected("bar bar")
                                .setArgument("foo", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar} {{bar {$foo}}}")
                                .setExpected("bar foo")
                                .setArgument("bar", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number} {{bar {$foo}}}")
                                .setExpected("bar 4.2")
                                .setArgument("bar", 4.2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number minimumFractionDigits=2} {{bar {$foo}}}")
                                .setExpected("bar 4.20")
                                .setArgument("bar", 4.2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number} {{bar {$foo}}}")
                                .setExpected("bar {$bar}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .setArgument("bar", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$baz} .local $bar = {$foo} {{bar {$bar}}}")
                                .setExpectSuccess()
                                .setExpected("bar foo")
                                .setArgument("baz", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$foo} {{bar {$foo}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setExpected("bar foo")
                                .setArgument("foo", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // TODO(duplicates): currently the expected output is based on using
    // the last definition of the duplicate-declared variable;
    // perhaps it's better to remove all declarations for $foo before formatting.
    // however if https://github.com/unicode-org/message-format-wg/pull/704 lands,
    // it'll be a moot point since the output will be expected to be the fallback string
    // (This applies to the expected output for all the U_DUPLICATE_DECLARATION_ERROR tests)
    test = testBuilder.setPattern(".local $foo = {$foo} .local $foo = {42} {{bar {$foo}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setArgument("foo", "foo")
                                .setExpected("bar 42")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {42} .local $foo = {$foo} {{bar {$foo}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setExpected("bar 42")
                                .setArgument("foo", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // see TODO(duplicates)
    test = testBuilder.setPattern(".local $foo = {:unknown} .local $foo = {42} {{bar {$foo}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setExpected("bar 42")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // see TODO(duplicates)
    test = testBuilder.setPattern(".local $x = {42} .local $y = {$x} .local $x = {13} {{{$x} {$y}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setExpected("13 42")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

/*
  Shouldn't this be "bar {$bar}"?

    test = testBuilder.setPattern(".local $foo = {$bar} .local $bar = {$baz} {{bar {$foo}}}")
                                .setExpected("bar foo")
                                .setArgument("baz", "foo", errorCode)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
*/

    test = testBuilder.setPattern(".match {$foo :string}  |1| {{one}}  * {{other}}")
                                .setExpected("one")
                                .setExpectSuccess()
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number}  1 {{one}}  * {{other}}")
                                .setExpected("one")
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

/*
  This case can't be tested without a way to set the "foo" argument to null

    test = testBuilder.setPattern(".match {$foo :number}  1 {{one}}  * {{other}}")
                                .setExpected("other")
                                .setArgument("foo", "", errorCode)
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
*/

    test = testBuilder.setPattern(".match {$foo :number}  one {{one}}  * {{other}}")
                                .setExpected("one")
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number}  1 {{=1}}  one {{one}}  * {{other}}")
                                .setExpected("=1")
                                .setArgument("foo", "1")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number}  1 {{=1}}  one {{one}}  * {{other}}")
                                .setExpected("=1")
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number}  one {{one}}  1 {{=1}}  * {{other}}")
                                .setExpected("=1")
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number}  one one {{one one}}  one * {{one other}}  * * {{other}}")
                                .setExpected("one one")
                                .setArgument("foo", (int64_t) 1)
                                .setArgument("bar", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number}  one one {{one one}}  one * {{one other}}  * * {{other}}")
                                .setExpected("one other")
                                .setArgument("foo", (int64_t) 1)
                                .setArgument("bar", (int64_t) 2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number}  one one {{one one}}  one * {{one other}}  * * {{other}}")
                                .setExpected("other")
                                .setArgument("foo", (int64_t) 2)
                                .setArgument("bar", (int64_t) 2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {|foo| :string} *{{foo}}")
                      .setExpectSuccess()
                      .setExpected("foo")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);


    test = testBuilder.setPattern(".local $foo = {$bar :number} .match {$foo}  one {{one}}  * {{other}}")
                                .setExpected("one")
                                .setArgument("bar", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number} .match {$foo}  one {{one}}  * {{other}}")
                                .setExpected("other")
                                .setArgument("bar", (int64_t) 2)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $bar = {$none} .match {$foo :number}  one {{one}}  * {{{$bar}}}")
                                .setExpected("one")
                                .setArgument("foo", (int64_t) 1)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

/*
  Note: this differs from https://github.com/messageformat/messageformat/blob/e0087bff312d759b67a9129eac135d318a1f0ce7/packages/mf2-messageformat/src/__fixtures/test-messages.json#L197

  The expected value in the test as defined there is "{$bar}".
  The value should be "{$none}" per 
https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#fallback-resolution -
" an error occurs in an expression with a variable operand and the variable refers to a local declaration, the fallback value is formatted based on the expression on the right-hand side of the declaration, rather than the expression in the selector or pattern."
*/
    test = testBuilder.setPattern(".local $bar = {$none} .match {$foo :number}  one {{one}}  * {{{$bar}}}")
                                .setExpected("{$none}")
                                .setArgument("foo", (int64_t) 2)
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Missing '$' before `bar`
    test = testBuilder.setPattern(".local bar = {|foo|} {{{$bar}}}")
                                .setExpected("{$bar}")
                                .clearArguments()
                                .setExpectedError(U_MF_SYNTAX_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Missing '=' after `bar`
    /*
      Spec is ambiguous -- see https://github.com/unicode-org/message-format-wg/issues/703 --
      but we choose the '{$bar}' interpretation for the partial result
     */
    test = testBuilder.setPattern(".local $bar {|foo|} {{{$bar}}}")
                                .setExpected("{$bar}")
                                .setExpectedError(U_MF_SYNTAX_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Missing '{'/'}' around `foo`
    test = testBuilder.setPattern(".local $bar = |foo| {{{$bar}}}")
                                .setExpected("{$bar}")
                                .setExpectedError(U_MF_SYNTAX_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Markup is ignored when formatting to string
    test = testBuilder.setPattern("{#tag}")
                                .setExpectSuccess()
                                .setExpected("")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag/}")
                                .setExpected("")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{/tag}")
                                .setExpected("")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag}content")
                      .setExpected("content")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag}content{/tag}")
                      .setExpected("content")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{/tag}content")
                      .setExpected("content")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag foo=bar}")
                      .setExpected("")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag foo=bar/}")
                      .setExpected("")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag foo=|foo| bar=$bar}")
                      .setArgument("bar", "b a r")
                      .setExpected("")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{/tag foo=bar}")
                      .setExpected("")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("no braces")
                      .setExpected("no braces")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("no braces {$foo}")
                      .setExpected("no braces 2")
                      .setArgument("foo", (int64_t) 2)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{{missing end brace")
                      .setExpected("missing end brace")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{{missing end {$brace")
                      .setExpected("missing end {$brace}")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{extra} content")
                      .setExpected("extra content")
                      .setExpectSuccess()
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{{extra}} content")
                      .setExpected("extra") // Everything after the closing '{{' should be ignored
                                            // per the `complex-body- production in the grammar
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // "empty \0xfffd"
    static constexpr UChar emptyWithReplacement[] = {
        0x65, 0x6D, 0x70, 0x74, 0x79, 0x20, REPLACEMENT, 0
    };

    test = testBuilder.setPattern("empty { }")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .setExpected(UnicodeString(emptyWithReplacement))
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{{bad {:}}")
                      .setExpected("bad {:}")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("unquoted {literal}")
                      .setExpected("unquoted literal")
                      .setExpectSuccess()
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(CharsToUnicodeString("bad {\\u0000placeholder}"))
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("no-equal {|42| :number minimumFractionDigits 2}")
                      .setExpected("no-equal 42.00")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("bad {:placeholder option=}")
                      .setExpected("bad {:placeholder}")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("bad {:placeholder option value}")
                      .setExpected("bad {:placeholder}")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("bad {:placeholder option}")
                      .setExpected("bad {:placeholder}")
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("bad {$placeholder option}")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("no {$placeholder end")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {}  * {{foo}}")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // "empty \0xfffd"
    static constexpr UChar replacement[] = {
        REPLACEMENT, 0
    };

    test = testBuilder.setPattern(".match {#foo}  * {{foo}}")
                      .setExpected(UnicodeString(replacement))
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match  * {{foo}}")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {|x|}  * foo")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {|x|}  * {{foo}} extra")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match |x|  * {{foo}}")
                      .clearExpected()
                      .setExpectedError(U_MF_SYNTAX_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number}  * * {{foo}}")
                      .clearExpected()
                      .setExpectedError(U_MF_VARIANT_KEY_MISMATCH_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number}  * {{foo}}")
                      .clearExpected()
                      .setExpectedError(U_MF_VARIANT_KEY_MISMATCH_ERROR)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}


/*
From https://github.com/unicode-org/message-format-wg/tree/main/test ,
alpha version

*/
void TestMessageFormat2::runSpecTests(IcuTestErrorCode& errorCode) {
    TestCase::Builder testBuilder;
    testBuilder.setName("specTests");

    TestCase test = testBuilder.setPattern("hello {world}")
        .setExpected("hello world")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello { world\t\n}")
                                .setExpected("hello world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // TODO:
    // For some reason, this test fails on Windows if
    // `pattern` is replaced with "hello {\\u3000world\r}".
    UnicodeString pattern("hello {");
    pattern += ((UChar32) 0x3000);
    pattern += "world\r}";
    test = testBuilder.setPattern(pattern)
                                .setExpected("hello world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {$place-.}")
                                .setExpected("hello world")
                                .setArgument("place-.", "world")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo} .local $bar = {$foo} {{bar {$bar}}}")
                                .setExpected("bar foo")
                                .setArgument("foo", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo} .local $bar = {$foo} {{bar {$bar}}}")
                                .setExpected("bar foo")
                                .setArgument("foo", "foo")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $x = {42} .local $y = {$x} {{{$x} {$y}}}")
                                 .setExpected("42 42")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag}")
                                 .setExpected("")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag}content")
                                 .setExpected("content")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#ns:tag}content{/ns:tag}")
                                 .setExpected("content")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{/tag}content")
                                 .setExpected("content")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag foo=bar}")
                                 .setExpected("")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{#tag a:foo=|foo| b:bar=$bar}")
                                 .setArgument("bar", "b a r")
                                 .setExpected("")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /*
    test = testBuilder.setPattern("{42 @foo @bar=13}")
                                 .clearArguments()
                                 .setExpected("42")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{42 @foo=$bar}")
                                 .setExpected("42")
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);
    */

    test = testBuilder.setPattern("foo {+reserved}")
                                 .setExpected("foo {+}")
                                 .setExpectedError(U_MF_UNSUPPORTED_EXPRESSION_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("foo {&private}")
                                 .setExpected("foo {&}")
                                 .setExpectedError(U_MF_UNSUPPORTED_EXPRESSION_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("foo {?reserved @a @b=$c}")
                                 .setExpected("foo {?}")
                                 .setExpectedError(U_MF_UNSUPPORTED_EXPRESSION_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".foo {42} {{bar}}")
                                 .setExpected("bar")
                                 .setExpectedError(U_MF_UNSUPPORTED_STATEMENT_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".foo {42}{{bar}}")
                                 .setExpected("bar")
                                 .setExpectedError(U_MF_UNSUPPORTED_STATEMENT_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".foo |}lit{| {42}{{bar}}")
                                 .setExpected("bar")
                                 .setExpectedError(U_MF_UNSUPPORTED_STATEMENT_ERROR)
                                 .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /* var2 is implicitly declared and can't be overridden by the second `.input` */
    test = testBuilder.setPattern(".input {$var :number minimumFractionDigits=$var2} .input {$var2 :number minimumFractionDigits=5} {{{$var} {$var2}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setArgument("var", (int64_t) 1)
                                .setArgument("var2", (int64_t) 3)
        // Note: the more "correct" fallback output seems like it should be "1.000 3" (ignoring the
        // overriding .input binding of $var2) but that's hard to achieve
        // as so-called "implicit declarations" can only be detected after parsing, at which
        // point the data model can't be modified.
        // Probably this is going to change anyway so that any data model error gets replaced
        // with a fallback for the whole message.
                                .setExpected("1.000 3.00000")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /* var2 is implicitly declared and can't be overridden by the second `.local` */
    test = testBuilder.setPattern(".local $var = {$var2} .local $var2 = {1} {{{$var} {$var2}}}")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .setArgument("var2", (int64_t) 5)
        // Same comment as above about the output
                                .setExpected("5 1")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /* var2 is provided as an argument but not used, and should have no effect on formatting */
    test = testBuilder.setPattern(".local $var2 = {1} {{{$var2}}}")
                                .setExpectSuccess()
                                .setArgument("var2", (int64_t) 5)
                                .setExpected("1")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Functions: integer
    test = testBuilder.setPattern("hello {4.2 :integer}")
                                .setExpectSuccess()
                                .setExpected("hello 4")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {-4.20 :integer}")
                                .setExpectSuccess()
                                .setExpected("hello -4")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {0.42e+1 :integer}")
                                .setExpectSuccess()
                                .setExpected("hello 4")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :integer} one {{one}} * {{other}}")
                                .setArgument("foo", 1.2)
                                .setExpectSuccess()
                                .setExpected("one")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Functions: number (formatting)

    // TODO: Need more test coverage for all the :number and other built-in
    // function options

    test = testBuilder.setPattern("hello {4.2 :number}")
                                .setExpectSuccess()
                                .setExpected("hello 4.2")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {-4.20 :number}")
                                .setExpectSuccess()
                                .setExpected("hello -4.2")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {0.42e+1 :number}")
                                .setExpectSuccess()
                                .setExpected("hello 4.2")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {foo :number}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .setExpected("hello {|foo|}")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {:number}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .setExpected("hello {:number}")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {4.2 :number minimumFractionDigits=2}")
                                .setExpectSuccess()
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {4.2 :number minimumFractionDigits=|2|}")
                                .setExpectSuccess()
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {4.2 :number minimumFractionDigits=$foo}")
                                .setExpectSuccess()
                                .setArgument("foo", (int64_t) 2)
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {|4.2| :number minimumFractionDigits=$foo}")
                                .setExpectSuccess()
                                .setArgument("foo", (int64_t) 2)
                                .setExpected("hello 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number} {{bar {$foo}}}")
                                .setExpectSuccess()
                                .setArgument("bar", 4.2)
                                .setExpected("bar 4.2")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number minimumFractionDigits=2} {{bar {$foo}}}")
                                .setExpectSuccess()
                                .setArgument("bar", 4.2)
                                .setExpected("bar 4.20")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /*
      This is underspecified -- commented out until https://github.com/unicode-org/message-format-wg/issues/738
      is resolved

    test = testBuilder.setPattern(".local $foo = {$bar :number minimumFractionDigits=foo} {{bar {$foo}}}")
                                .setExpectedError(U_MF_FORMATTING_ERROR)
                                .setArgument("bar", 4.2)
                                .setExpected("bar {$bar}")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    */

    test = testBuilder.setPattern(".local $foo = {$bar :number} {{bar {$foo}}}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setArgument("bar", "foo")
                                  .setExpected("bar {$bar}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo :number} {{bar {$foo}}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", 4.2)
                                  .setExpected("bar 4.2")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo :number minimumFractionDigits=2} {{bar {$foo}}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", 4.2)
                                  .setExpected("bar 4.20")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    /*
    This is underspecified -- commented out until https://github.com/unicode-org/message-format-wg/issues/738
      is resolved

    test = testBuilder.setPattern(".input {$foo :number minimumFractionDigits=foo} {{bar {$foo}}}")
                                .setExpectedError(U_MF_FORMATTING_ERROR)
                                .setArgument("foo", 4.2)
                                .setExpected("bar {$foo}")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    */

    test = testBuilder.setPattern(".input {$foo :number} {{bar {$foo}}}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setArgument("foo", "foo")
                                  .setExpected("bar {$foo}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Functions: number (selection)

    test = testBuilder.setPattern(".match {$foo :number} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} 1 {{=1}} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("=1")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} one {{one}} 1 {{=1}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("=1")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number} one one {{one one}} one * {{one other}} * * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setArgument("bar", (int64_t) 1)
                                  .setExpected("one one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number} one one {{one one}} one * {{one other}} * * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setArgument("bar", (int64_t) 2)
                                  .setExpected("one other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :number} {$bar :number} one one {{one one}} one * {{one other}} * * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 2)
                                  .setArgument("bar", (int64_t) 2)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo :number} .match {$foo} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $foo = {$bar :number} .match {$foo} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$foo :number} .local $bar = {$foo} .match {$bar} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$bar :number} .match {$bar} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 2)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$bar} .match {$bar :number} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$bar} .match {$bar :number} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 2)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$bar} .match {$bar :number} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$bar} .match {$bar :number} one {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("bar", (int64_t) 2)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".input {$none} .match {$foo :number} one {{one}} * {{{$none}}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $bar = {$none} .match {$foo :number} one {{one}} * {{{$bar}}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $bar = {$none} .match {$foo :number} one {{one}} * {{{$bar}}}")
                                  .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                  .setArgument("foo", (int64_t) 2)
                                  .setExpected("{$none}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{42 :number @foo @bar=13}")
                                  .setExpectSuccess()
                                  .setExpected("42")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Neither `ordinal` nor `selectordinal` exists in this spec version
    test = testBuilder.setPattern(".match {$foo :ordinal} one {{st}} two {{nd}} few {{rd}} * {{th}}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("th")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {42 :ordinal}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setExpected("hello {|42|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :stringordinal} one {{st}} two {{nd}} few {{rd}} * {{th}}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("th")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {42 :stringordinal}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setExpected("hello {|42|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);


    // Same for `plural`

    test = testBuilder.setPattern(".match {$foo :plural} one {{one}} * {{other}}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("hello {42 :plural}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setExpected("hello {|42|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // :string

    test = testBuilder.setPattern(".match {$foo :string} |1| {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :string} 1 {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("one")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // The spec test with argument "foo" set to null is omitted, since
    // this implementation doesn't support null arguments

    test = testBuilder.setPattern(".match {$foo :string} 1 {{one}} * {{other}}")
                                  .setExpectSuccess()
                                  .setArgument("foo", (double) 42.5)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".match {$foo :string} 1 {{one}} * {{other}}")
                                  .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                  .clearArguments()
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);


    // There is no `:select` in this version of the spec
    test = testBuilder.setPattern(".match {$foo :select} one {{one}} * {{other}}")
                                  .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                  .setArgument("foo", (int64_t) 1)
                                  .setExpected("other")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // :date
    test = testBuilder.setPattern("{:date}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{:date}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{horse :date}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{|horse|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02| :date}")
                                  .setExpectSuccess()
                                  .setExpected("1/2/06")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :date}")
                                  .setExpectSuccess()
                                  .setExpected("1/2/06")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02| :date style=long}")
                                  .setExpectSuccess()
                                  .setExpected("January 2, 2006")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $d = {|2006-01-02| :date style=long} {{{$d :date}}}")
                                  .setExpectSuccess()
                                  .setExpected("January 2, 2006")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $t = {|2006-01-02T15:04:06| :time} {{{$t :date}}}")
                                  .setExpectSuccess()
                                  .setExpected("1/2/06")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // :time
    test = testBuilder.setPattern("{:time}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{:time}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{horse :time}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{|horse|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :time}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("3:04\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :time style=medium}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("3:04:06\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $t = {|2006-01-02T15:04:06| :time style=medium} {{{$t :time}}}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("3:04:06\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $t = {|2006-01-02T15:04:06| :date} {{{$t :time}}}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("3:04\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);


    // :datetime
    test = testBuilder.setPattern("{:datetime}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{:datetime}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{$x :datetime}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{$x}")
                                  .setArgument("x", (int64_t) 1)
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{$x :datetime}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{$x}")
                                  .setArgument("x", "true")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{horse :datetime}")
                                  .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                  .setExpected("{|horse|}")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :datetime}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("1/2/06, 3:04\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :datetime year=numeric month=|2-digit|}")
                                  .setExpectSuccess()
                                  .setExpected("01/2006")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :datetime dateStyle=long}")
                                  .setExpectSuccess()
                                  .setExpected("January 2, 2006")
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{|2006-01-02T15:04:06| :datetime timeStyle=medium}")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("3:04:06\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("{$dt :datetime}")
                                  .setArgument("dt", "2006-01-02T15:04:06")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("1/2/06, 3:04\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);

/*
TODO
This can't work -- the "style" option is different from "dateStyle" and can't get used
in the second call to `:datetime`
See https://github.com/unicode-org/message-format-wg/issues/726

    test = testBuilder.setPattern(".input {$dt :time style=medium} {{{$dt :datetime dateStyle=long}}}")
                                  .setArgument("dt", "2006-01-02T15:04:06")
                                  .setExpectSuccess()
                                  .setExpected(CharsToUnicodeString("January 2, 2006 at 3:04:06\\u202FPM"))
                                  .build();
    TestUtils::runTestCase(*this, test, errorCode);
*/

    // TODO: tests for other function options?
}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
