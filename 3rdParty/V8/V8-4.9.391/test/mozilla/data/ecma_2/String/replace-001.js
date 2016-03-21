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
 * Netscape Communication Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

gTestfile = 'replace-001.js';

/**
 *  File Name:          String/replace-001.js
 *  ECMA Section:       15.6.4.10
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */

var SECTION = "String/replace-001.js";
var VERSION = "ECMA_2";
var TITLE   = "String.prototype.replace( regexp, replaceValue )";

startTest();

/*
 * If regexp is not an object of type RegExp, it is replaced with the
 * result of the expression new RegExp(regexp).  Let string denote the
 * result of converting the this value to a string.  String is searched
 * for the first occurrence of the regular expression pattern regexp if
 * regexp.global is false, or all occurrences if regexp.global is true.
 *
 * The match is performed as in String.prototype.match, including the
 * update of regexp.lastIndex.  Let m be the number of matched
 * parenthesized subexpressions as specified in section 15.7.5.3.
 *
 * If replaceValue is a function, then for each matched substring, call
 * the function with the following m + 3 arguments. Argument 1 is the
 * substring that matched. The next m arguments are all of the matched
 * subexpressions. Argument m + 2 is the length of the left context, and
 * argument m + 3 is string.
 *
 * The result is a string value derived from the original input by
 * replacing each matched substring with the corresponding return value
 * of the function call, converted to a string if need be.
 *
 * Otherwise, let newstring denote the result of converting replaceValue
 * to a string. The result is a string value derived from the original
 * input string by replacing each matched substring with a string derived
 * from newstring by replacing characters in newstring by replacement text
 * as specified in the following table:
 *
 * $& The matched substring.
 * $‘ The portion of string that precedes the matched substring.
 * $’ The portion of string that follows the matched substring.
 * $+ The substring matched by the last parenthesized subexpressions in
 *      the regular expression.
 * $n The corresponding matched parenthesized subexpression n, where n
 * is a single digit 0-9. If there are fewer than n subexpressions, “$n
 * is left unchanged.
 *
 * Note that the replace function is intentionally generic; it does not
 * require that its this value be a string object. Therefore, it can be
 * transferred to other kinds of objects for use as a method.
 */


AddTestCase( "NO TESTS EXIST", "PASSED", "Test not implemented");

test();
