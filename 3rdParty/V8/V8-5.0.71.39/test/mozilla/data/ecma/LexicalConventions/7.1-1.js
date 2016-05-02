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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
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

gTestfile = '7.1-1.js';

/**
   File Name:          7.1-1.js
   ECMA Section:       7.1 White Space
   Description:        - readability
   - separate tokens
   - otherwise should be insignificant
   - in strings, white space characters are significant
   - cannot appear within any other kind of token

   white space characters are:
   unicode     name            formal name     string representation
   \u0009      tab             <TAB>           \t
   \u000B      veritical tab   <VT>            \v
   \U000C      form feed       <FF>            \f
   \u0020      space           <SP>            " "

   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "7.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "White Space";

writeHeaderToLog( SECTION + " "+ TITLE);

// whitespace between var keyword and identifier

new TestCase( SECTION,  'var'+'\t'+'MYVAR1=10;MYVAR1',   10, eval('var'+'\t'+'MYVAR1=10;MYVAR1') );
new TestCase( SECTION,  'var'+'\f'+'MYVAR2=10;MYVAR2',   10, eval('var'+'\f'+'MYVAR2=10;MYVAR2') );
new TestCase( SECTION,  'var'+'\v'+'MYVAR2=10;MYVAR2',   10, eval('var'+'\v'+'MYVAR2=10;MYVAR2') );
new TestCase( SECTION,  'var'+'\ '+'MYVAR2=10;MYVAR2',   10, eval('var'+'\ '+'MYVAR2=10;MYVAR2') );

// use whitespace between tokens object name, dot operator, and object property

new TestCase( SECTION,
	      "var a = new Array(12345); a\t\v\f .\\u0009\\000B\\u000C\\u0020length",
	      12345,
	      eval("var a = new Array(12345); a\t\v\f .\u0009\u0020\u000C\u000Blength") );

test();
