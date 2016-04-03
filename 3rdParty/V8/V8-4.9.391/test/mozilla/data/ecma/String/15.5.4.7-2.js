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

gTestfile = '15.5.4.7-2.js';

/**
   File Name:          15.5.4.7-2.js
   ECMA Section:       15.5.4.7 String.prototype.lastIndexOf( searchString, pos)
   Description:

   If the given searchString appears as a substring of the result of
   converting this object to a string, at one or more positions that are
   at or to the left of the specified position, then the index of the
   rightmost such position is returned; otherwise -1 is returned. If position
   is undefined or not supplied, the length of this string value is assumed,
   so as to search all of the string.

   When the lastIndexOf method is called with two arguments searchString and
   position, the following steps are taken:

   1.Call ToString, giving it the this value as its argument.
   2.Call ToString(searchString).
   3.Call ToNumber(position). (If position is undefined or not supplied, this step produces the value NaN).
   4.If Result(3) is NaN, use +; otherwise, call ToInteger(Result(3)).
   5.Compute the number of characters in Result(1).
   6.Compute min(max(Result(4), 0), Result(5)).
   7.Compute the number of characters in the string that is Result(2).
   8.Compute the largest possible integer k not larger than Result(6) such that k+Result(7) is not greater
   than Result(5), and for all nonnegative integers j less than Result(7), the character at position k+j of
   Result(1) is the same as the character at position j of Result(2); but if there is no such integer k, then
   compute the value -1.

   1.Return Result(8).

   Note that the lastIndexOf function is intentionally generic; it does not require that its this value be a
   String object. Therefore it can be transferred to other kinds of objects for use as a method.

   Author:             christine@netscape.com, pschwartau@netscape.com
   Date:               02 October 1997
   Modified:           14 July 2002
   Reason:             See http://bugzilla.mozilla.org/show_bug.cgi?id=155289
   ECMA-262 Ed.3  Section 15.5.4.8
   The length property of the lastIndexOf method is 1
   *
   */
var SECTION = "15.5.4.7-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.protoype.lastIndexOf";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION, "String.prototype.lastIndexOf.length",           1,          String.prototype.lastIndexOf.length );
new TestCase( SECTION, "delete String.prototype.lastIndexOf.length",    false,      delete String.prototype.lastIndexOf.length );
new TestCase( SECTION, "delete String.prototype.lastIndexOf.length; String.prototype.lastIndexOf.length",   1,  eval("delete String.prototype.lastIndexOf.length; String.prototype.lastIndexOf.length" ) );

new TestCase( SECTION, "var s = new String(''); s.lastIndexOf('', 0)",          LastIndexOf("","",0),  eval("var s = new String(''); s.lastIndexOf('', 0)") );
new TestCase( SECTION, "var s = new String(''); s.lastIndexOf('')",             LastIndexOf("",""),  eval("var s = new String(''); s.lastIndexOf('')") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('', 0)",     LastIndexOf("hello","",0),  eval("var s = new String('hello'); s.lastIndexOf('',0)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('')",        LastIndexOf("hello",""),  eval("var s = new String('hello'); s.lastIndexOf('')") );

new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll')",     LastIndexOf("hello","ll"),  eval("var s = new String('hello'); s.lastIndexOf('ll')") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 0)",  LastIndexOf("hello","ll",0),  eval("var s = new String('hello'); s.lastIndexOf('ll', 0)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 1)",  LastIndexOf("hello","ll",1),  eval("var s = new String('hello'); s.lastIndexOf('ll', 1)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 2)",  LastIndexOf("hello","ll",2),  eval("var s = new String('hello'); s.lastIndexOf('ll', 2)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 3)",  LastIndexOf("hello","ll",3),  eval("var s = new String('hello'); s.lastIndexOf('ll', 3)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 4)",  LastIndexOf("hello","ll",4),  eval("var s = new String('hello'); s.lastIndexOf('ll', 4)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 5)",  LastIndexOf("hello","ll",5),  eval("var s = new String('hello'); s.lastIndexOf('ll', 5)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 6)",  LastIndexOf("hello","ll",6),  eval("var s = new String('hello'); s.lastIndexOf('ll', 6)") );

new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 1.5)", LastIndexOf('hello','ll', 1.5), eval("var s = new String('hello'); s.lastIndexOf('ll', 1.5)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', 2.5)", LastIndexOf('hello','ll', 2.5),  eval("var s = new String('hello'); s.lastIndexOf('ll', 2.5)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', -1)",  LastIndexOf('hello','ll', -1), eval("var s = new String('hello'); s.lastIndexOf('ll', -1)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', -1.5)",LastIndexOf('hello','ll', -1.5), eval("var s = new String('hello'); s.lastIndexOf('ll', -1.5)") );

new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', -Infinity)",    LastIndexOf("hello","ll",-Infinity), eval("var s = new String('hello'); s.lastIndexOf('ll', -Infinity)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', Infinity)",    LastIndexOf("hello","ll",Infinity), eval("var s = new String('hello'); s.lastIndexOf('ll', Infinity)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', NaN)",    LastIndexOf("hello","ll",NaN), eval("var s = new String('hello'); s.lastIndexOf('ll', NaN)") );
new TestCase( SECTION, "var s = new String('hello'); s.lastIndexOf('ll', -0)",    LastIndexOf("hello","ll",-0), eval("var s = new String('hello'); s.lastIndexOf('ll', -0)") );
for ( var i = 0; i < ( "[object Object]" ).length; i++ ) {
  new TestCase(   SECTION,
		  "var o = new Object(); o.lastIndexOf = String.prototype.lastIndexOf; o.lastIndexOf('b', "+ i + ")",
		  ( i < 2 ? -1 : ( i < 9  ? 2 : 9 )) ,
		  eval("var o = new Object(); o.lastIndexOf = String.prototype.lastIndexOf; o.lastIndexOf('b', "+ i + ")") );
}
for ( var i = 0; i < 5; i ++ ) {
  new TestCase(   SECTION,
		  "var b = new Boolean(); b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('l', "+ i + ")",
		  ( i < 2 ? -1 : 2 ),
		  eval("var b = new Boolean(); b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('l', "+ i + ")") );
}
for ( var i = 0; i < 5; i ++ ) {
  new TestCase(   SECTION,
		  "var b = new Boolean(); b.toString = Object.prototype.toString; b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('o', "+ i + ")",
		  ( i < 1 ? -1 : ( i < 9 ? 1 : ( i < 10 ? 9 : 10 ) ) ),
		  eval("var b = new Boolean();  b.toString = Object.prototype.toString; b.lastIndexOf = String.prototype.lastIndexOf; b.lastIndexOf('o', "+ i + ")") );
}
for ( var i = 0; i < 9; i++ ) {
  new TestCase(   SECTION,
		  "var n = new Number(Infinity); n.lastIndexOf = String.prototype.lastIndexOf; n.lastIndexOf( 'i', " + i + " )",
		  ( i < 3 ? -1 : ( i < 5 ? 3 : 5 ) ),
		  eval("var n = new Number(Infinity); n.lastIndexOf = String.prototype.lastIndexOf; n.lastIndexOf( 'i', " + i + " )") );
}
var a = new Array( "abc","def","ghi","jkl","mno","pqr","stu","vwx","yz" );

for ( var i = 0; i < (a.toString()).length; i++ ) {
  new TestCase( SECTION,
		"var a = new Array( 'abc','def','ghi','jkl','mno','pqr','stu','vwx','yz' ); a.lastIndexOf = String.prototype.lastIndexOf; a.lastIndexOf( ',mno,p', "+i+" )",
		( i < 15 ? -1 : 15 ),
		eval("var a = new Array( 'abc','def','ghi','jkl','mno','pqr','stu','vwx','yz' ); a.lastIndexOf = String.prototype.lastIndexOf; a.lastIndexOf( ',mno,p', "+i+" )") );
}

for ( var i = 0; i < 15; i ++ ) {
  new TestCase(   SECTION,
		  "var m = Math; m.lastIndexOf = String.prototype.lastIndexOf; m.lastIndexOf('t', "+ i + ")",
		  ( i < 6 ? -1 : ( i < 10 ? 6 : 10 ) ),
		  eval("var m = Math; m.lastIndexOf = String.prototype.lastIndexOf; m.lastIndexOf('t', "+ i + ")") );
}
/*
  for ( var i = 0; i < 15; i++ ) {
  new TestCase(   SECTION,
  "var d = new Date(); d.lastIndexOf = String.prototype.lastIndexOf; d.lastIndexOf( '0' )",
  )
  }

*/

test();

function LastIndexOf( string, search, position ) {
  string = String( string );
  search = String( search );

  position = Number( position )

    if ( isNaN( position ) ) {
      position = Infinity;
    } else {
      position = ToInteger( position );
    }

  result5= string.length;
  result6 = Math.min(Math.max(position, 0), result5);
  result7 = search.length;

  if (result7 == 0) {
    return Math.min(position, result5);
  }

  result8 = -1;

  for ( k = 0; k <= result6; k++ ) {
    if ( k+ result7 > result5 ) {
      break;
    }
    for ( j = 0; j < result7; j++ ) {
      if ( string.charAt(k+j) != search.charAt(j) ){
	break;
      }   else  {
	if ( j == result7 -1 ) {
	  result8 = k;
	}
      }
    }
  }

  return result8;
}
function ToInteger( n ) {
  n = Number( n );
  if ( isNaN(n) ) {
    return 0;
  }
  if ( Math.abs(n) == 0 || Math.abs(n) == Infinity ) {
    return n;
  }

  var sign = ( n < 0 ) ? -1 : 1;

  return ( sign * Math.floor(Math.abs(n)) );
}
