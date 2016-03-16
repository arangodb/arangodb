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

gTestfile = '15.5.4.4-3.js';

/**
   File Name:          15.5.4.4-3.js
   ECMA Section:       15.5.4.4 String.prototype.charAt(pos)
   Description:        Returns a string containing the character at position
   pos in the string.  If there is no character at that
   string, the result is the empty string.  The result is
   a string value, not a String object.

   When the charAt method is called with one argument,
   pos, the following steps are taken:
   1. Call ToString, with this value as its argument
   2. Call ToInteger pos
   3. Compute the number of characters  in Result(1)
   4. If Result(2) is less than 0 is or not less than
   Result(3), return the empty string
   5. Return a string of length 1 containing one character
   from result (1), the character at position Result(2).

   Note that the charAt function is intentionally generic;
   it does not require that its this value be a String
   object.  Therefore it can be transferred to other kinds
   of objects for use as a method.

   This tests assiging charAt to a user-defined function.

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.4-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.charAt";

writeHeaderToLog( SECTION + " "+ TITLE);

var foo = new MyObject('hello');


new TestCase( SECTION, "var foo = new MyObject('hello'); ", "h", foo.charAt(0)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "e", foo.charAt(1)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "l", foo.charAt(2)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "l", foo.charAt(3)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "o", foo.charAt(4)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "",  foo.charAt(-1)  );
new TestCase( SECTION, "var foo = new MyObject('hello'); ", "",  foo.charAt(5)  );

var boo = new MyObject(true);

new TestCase( SECTION, "var boo = new MyObject(true); ", "t", boo.charAt(0)  );
new TestCase( SECTION, "var boo = new MyObject(true); ", "r", boo.charAt(1)  );
new TestCase( SECTION, "var boo = new MyObject(true); ", "u", boo.charAt(2)  );
new TestCase( SECTION, "var boo = new MyObject(true); ", "e", boo.charAt(3)  );

var noo = new MyObject( Math.PI );

new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "3", noo.charAt(0)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", ".", noo.charAt(1)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "1", noo.charAt(2)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "4", noo.charAt(3)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "1", noo.charAt(4)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "5", noo.charAt(5)  );
new TestCase( SECTION, "var noo = new MyObject(Math.PI); ", "9", noo.charAt(6)  );

test();

function MyObject (v) {
  this.value      = v;
  this.toString   = new Function( "return this.value +'';" );
  this.valueOf    = new Function( "return this.value" );
  this.charAt     = String.prototype.charAt;
}

