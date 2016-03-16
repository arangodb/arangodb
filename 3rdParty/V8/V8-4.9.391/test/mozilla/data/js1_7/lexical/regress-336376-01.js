/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTestfile = 'regress-336376-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER     = "336376";
var summary = "Tests reserved words in contexts in which they are not reserved";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * TEST SETUP *
 **************/

//
// New tests go in Tester.prototype._tests.  A test is called with a single
// argument, the keyword to test in the syntax tested by that test.  Tests
// should not return anything, and they should signal failure by throwing an
// explanatory exception and success by not throwing one.
//
// If you define a new test, make sure to name it using an informative string
// for ease of use if any keywords ever manually define the array of tests they
// should pass, and add it as a string to ALL_TESTS.
//

// all tests
const ALL_TESTS =
  [
    "CONTEXT_OBJECT_LITERAL_PROPERTY",
    "CONTEXT_OBJECT_PROPERTY_DOT_REFERENCE",
    "CONTEXT_OBJECT_PROPERTY_DOT_REFERENCE_IS_FUNCTION",
    "CONTEXT_OBJECT_PROPERTY_DOT_GET",
    "CONTEXT_OBJECT_PROPERTY_DOT_SET",
    "CONTEXT_XML_DESCENDANTS",
    "CONTEXT_XML_NAMESPACE_QUALIFIED_ELEMENT",
    "CONTEXT_XML_NAMESPACE_QUALIFIED_ATTR",
    "CONTEXT_XML_ATTRIBUTE_SELECTOR",
    ];

function r(keyword, tests)
{
  /**
   * @param keyword
   *   the keyword as a string
   * @param tests
   *   array of test numbers against it, or leave undefined to run all tests
   *   against it
   */
  function Reserved(keyword, tests)
  {
    this.keyword = keyword;
    if (tests)
      this.tests = tests;
    else
      this.tests = ALL_TESTS;
  }
  Reserved.prototype =
    {
      toString:
      function()
      {
	return "'" + this.keyword + "' being run against tests " +
	this.tests;
      }
    };
  return new Reserved(keyword, tests);
}

// ECMA-262, 3rd. ed. keywords -- see 7.5.2
const ECMA_262_3_KEYWORD =
  [
    r("break"),
    r("case"),
    r("catch"),
    r("continue"),
    r("default"),
    r("delete"),
    r("do"),
    r("else"),
    r("finally"),
    r("for"),
    r("function"),
    r("if"),
    r("in"),
    r("instanceof"),
    r("new"),
    r("return"),
    r("switch"),
    r("this"),
    r("throw"),
    r("try"),
    r("typeof"),
    r("var"),
    r("void"),
    r("while"),
    r("with"),
    ];

// ECMA-262, 3rd. ed. future reserved keywords -- see 7.5.3
const ECMA_262_3_FUTURERESERVEDKEYWORD =
  [
    r("abstract"),
    r("boolean"),
    r("byte"),
    r("char"),
    r("class"),
    r("const"),
    r("debugger"),
    r("double"),
    r("enum"),
    r("export"),
    r("extends"),
    r("final"),
    r("float"),
    r("goto"),
    r("implements"),
    r("import"),
    r("int"),
    r("interface"),
    r("long"),
    r("native"),
    r("package"),
    r("private"),
    r("protected"),
    r("public"),
    r("short"),
    r("static"),
    r("super"),
    r("synchronized"),
    r("throws"),
    r("transient"),
    r("volatile"),
    ];

// like reserved words, but not quite reserved words
const PSEUDO_RESERVED =
  [
    r("true"),
    r("false"),
    r("null"),
    r("each"),  // |for each|
    ];

// new-in-ES4 reserved words -- fill this as each is implemented
const ECMA_262_4_RESERVED_WORDS =
  [
    r("let")
    ];



/**
 * @param keyword
 *   string containing the tested keyword
 * @param test
 *   the number of the failing test
 * @param error
 *   the exception thrown when running the test
 */
function Failure(keyword, test, error)
{
  this.keyword = keyword;
  this.test = test;
  this.error = error;
}
Failure.prototype =
{
  toString:
  function()
  {
    return "*** FAILURE on '" + this.keyword + "'!\n" +
    "* test:     " + this.test + "\n" +
    "* error:    " + this.error + "\n";
  }
};

function Tester()
{
  this._failedTests = [];
}
Tester.prototype =
{
  testReservedWords:
  function(reservedArray)
  {
    var rv;
    for (var i = 0, sz = reservedArray.length; i < sz; i++)
    {
      var res = reservedArray[i];
      if (!res)
	continue;

      var tests = res.tests;
      for (var j = 0, sz2 = tests.length; j < sz2; j++)
      {
	var test = tests[j];
	if (!test)
	  continue;

	try
	{
	  this._tests[test](res.keyword);
	}
	catch (e)
	{
	  this._failedTests.push(new Failure(res.keyword, test, e));
	}
      }
    }
  },
  flushErrors:
  function ()
  {
    if (this._failedTests.length > 0) {
      var except = "*************************\n" +
      "* FAILURES ENCOUNTERED! *\n" +
      "*************************\n";
      for (var i = 0, sz = this._failedTests.length; i < sz; i++)
	except += this._failedTests[i];
      throw except;
    }
  },
  _tests:
  {
    CONTEXT_OBJECT_LITERAL_PROPERTY:
    function(keyword)
    {
      try
      {
	eval("var o = { " + keyword + ": 17 };\n" +
	     "if (o['" + keyword + "'] != 17)\n" +
	     "throw \"o['" + keyword + "'] == 17\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_OBJECT_PROPERTY_DOT_REFERENCE:
    function(keyword)
    {
      try
      {
	eval("var o = { \"" + keyword + "\": 17, baz: null };\n" +
	     "if (o." + keyword + " != 17)\n" +
	     "throw \"o." + keyword + " == 17\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_OBJECT_PROPERTY_DOT_REFERENCE_IS_FUNCTION:
    function(keyword)
    {
      try
      {
	eval("var o = { '" + keyword + "': function() { return 17; }, baz: null };\n" +
	     "if (o." + keyword + "() != 17)\n" +
	     "throw \"o." + keyword + " == 17\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_OBJECT_PROPERTY_DOT_GET:
    function(keyword)
    {
      try
      {
	var o = {};
	eval("o['" + keyword + "'] = 17;\n" +
	     "if (o." + keyword + " != 17)\n" +
	     "throw \"'o." + keyword + " != 17' failed!\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_OBJECT_PROPERTY_DOT_SET:
    function(keyword)
    {
      try
      {
	var o = {};
	eval("o." + keyword + " = 17;\n" +
	     "if (o['" + keyword + "'] != 17)\n" +
	     "throw \"'o." + keyword + " = 17' failed!\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_XML_DESCENDANTS:
    function(keyword)
    {
      try
      {
	eval("var x = <foo><biz><" + keyword + " id='1'/></biz><" + keyword + " f='g'/></foo>;\n" +
	     "if (x.." + keyword + ".length() != 2 ||\n" +
	     "    x.." + keyword + " !=              \n" +
	     "      <><" + keyword + " id='1'/><" + keyword + " f='g'/></>)\n" +
	     "throw \"'x.." + keyword + ".length()' failed!\";");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_XML_NAMESPACE_QUALIFIED_ELEMENT:
    function(keyword)
    {
      try
      {
	var bar = new Namespace("http://localhost/");
	eval("var x = <foo xmlns:bar='http://localhost/'>\n\
                              <bar>\n\
                                <baz/>\n\
                              </bar>\n\
                              <bar:" + keyword + " id='17'/>\n\
                              <quiz>\n\
                                <bar:" + keyword + ">\n\
                                  <bunk/>\n\
                                </bar:" + keyword + ">\n\
                              </quiz>\n\
                            </foo>;\n\
                    if (x.quiz.bar::" + keyword + " != \n\
                          <bar:" + keyword + "  xmlns:bar='http://localhost/'>\n\
                            <bunk/>\n\
                          </bar:" + keyword + "> ||\n\
                        x..bar::" + keyword + " != \n\
                            <><bar:" + keyword + "\n\
                                xmlns:bar='http://localhost/' id='17'/>\n\
                              <bar:" + keyword + " xmlns:bar='http://localhost/'>\n\
                                <bunk/>\n\
                              </bar:" + keyword + "></>)\n\
                      throw 'reserved names in XML namespace-qualified stuff are broken!';");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_XML_NAMESPACE_QUALIFIED_ATTR:
    function(keyword)
    {
      try
      {
	var bar = new Namespace("http://localhost/");
	eval("var x = <foo xmlns:bar='http://localhost/'>\
                              <fin bar:" + keyword + "='5'/>\
                            </foo>;\n\
                    if (x.fin.@bar::" + keyword + " != 5)\n\
                      throw 'namespaced attributes which are keywords are broken!';");
      }
      catch (e)
      {
	throw e;
      }
    },
    CONTEXT_XML_ATTRIBUTE_SELECTOR:
    function(keyword)
    {
      try
      {
	eval("var x = <foo " + keyword + "='idref'/>;\n\
                    if (x.@" + keyword + " != 'idref')\n\
                      throw 'keywords on the right of the @ E4X selector are broken!';");
      }
      catch (e)
      {
	throw e;
      }
    }
  }
};


/***************
 * BEGIN TESTS *
 ***************/

var failed = false;

try
{
  var tester = new Tester();
  tester.testReservedWords(ECMA_262_3_KEYWORD);
  tester.testReservedWords(ECMA_262_3_FUTURERESERVEDKEYWORD);
  tester.testReservedWords(PSEUDO_RESERVED);
  tester.testReservedWords(ECMA_262_4_RESERVED_WORDS);
  tester.flushErrors();
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
