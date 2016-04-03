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

gTestfile = '15.1.2.5-1.js';

/**
   File Name:          15.1.2.5-1.js
   ECMA Section:       15.1.2.5  Function properties of the global object
   unescape( string )

   Description:
   The unescape function computes a new version of a string value in which
   each escape sequences of the sort that might be introduced by the escape
   function is replaced with the character that it represents.

   When the unescape function is called with one argument string, the
   following steps are taken:

   1.  Call ToString(string).
   2.  Compute the number of characters in Result(1).
   3.  Let R be the empty string.
   4.  Let k be 0.
   5.  If k equals Result(2), return R.
   6.  Let c be the character at position k within Result(1).
   7.  If c is not %, go to step 18.
   8.  If k is greater than Result(2)-6, go to step 14.
   9.  If the character at position k+1 within result(1) is not u, go to step
   14.
   10. If the four characters at positions k+2, k+3, k+4, and k+5 within
   Result(1) are not all hexadecimal digits, go to step 14.
   11. Let c be the character whose Unicode encoding is the integer represented
   by the four hexadecimal digits at positions k+2, k+3, k+4, and k+5
   within Result(1).
   12. Increase k by 5.
   13. Go to step 18.
   14. If k is greater than Result(2)-3, go to step 18.
   15. If the two characters at positions k+1 and k+2 within Result(1) are not
   both hexadecimal digits, go to step 18.
   16. Let c be the character whose Unicode encoding is the integer represented
   by two zeroes plus the two hexadecimal digits at positions k+1 and k+2
   within Result(1).
   17. Increase k by 2.
   18. Let R be a new string value computed by concatenating the previous value
   of R and c.
   19. Increase k by 1.
   20. Go to step 5.
   Author:             christine@netscape.com
   Date:               28 october 1997
*/

var SECTION = "15.1.2.5-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "unescape(string)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "unescape.length",       1,               unescape.length );
new TestCase( SECTION, "unescape.length = null; unescape.length",   1,      eval("unescape.length=null; unescape.length") );
new TestCase( SECTION, "delete unescape.length",                    false,  delete unescape.length );
new TestCase( SECTION, "delete unescape.length; unescape.length",   1,      eval("delete unescape.length; unescape.length") );
new TestCase( SECTION, "var MYPROPS=''; for ( var p in unescape ) { MYPROPS+= p }; MYPROPS",    "prototype", eval("var MYPROPS=''; for ( var p in unescape ) { MYPROPS+= p }; MYPROPS") );

new TestCase( SECTION, "unescape()",              "undefined",    unescape() );
new TestCase( SECTION, "unescape('')",            "",             unescape('') );
new TestCase( SECTION, "unescape( null )",        "null",         unescape(null) );
new TestCase( SECTION, "unescape( void 0 )",      "undefined",    unescape(void 0) );
new TestCase( SECTION, "unescape( true )",        "true",         unescape( true ) );
new TestCase( SECTION, "unescape( false )",       "false",        unescape( false ) );

new TestCase( SECTION, "unescape( new Boolean(true) )",   "true", unescape(new Boolean(true)) );
new TestCase( SECTION, "unescape( new Boolean(false) )",  "false",    unescape(new Boolean(false)) );

new TestCase( SECTION, "unescape( Number.NaN  )",                 "NaN",      unescape(Number.NaN) );
new TestCase( SECTION, "unescape( -0 )",                          "0",        unescape( -0 ) );
new TestCase( SECTION, "unescape( 'Infinity' )",                  "Infinity", unescape( "Infinity" ) );
new TestCase( SECTION, "unescape( Number.POSITIVE_INFINITY )",    "Infinity", unescape( Number.POSITIVE_INFINITY ) );
new TestCase( SECTION, "unescape( Number.NEGATIVE_INFINITY )",    "-Infinity", unescape( Number.NEGATIVE_INFINITY ) );

var ASCII_TEST_STRING = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@*_+-./";

new TestCase( SECTION, "unescape( " +ASCII_TEST_STRING+" )",    ASCII_TEST_STRING,  unescape( ASCII_TEST_STRING ) );

// escaped chars with ascii values less than 256

for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
  new TestCase( SECTION,
		"unescape( %"+ ToHexString(CHARCODE)+" )",
		String.fromCharCode(CHARCODE),
		unescape( "%" + ToHexString(CHARCODE) )  );
}

// unicode chars represented by two hex digits
for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
  new TestCase( SECTION,
		"unescape( %u"+ ToHexString(CHARCODE)+" )",
		"%u"+ToHexString(CHARCODE),
		unescape( "%u" + ToHexString(CHARCODE) )  );
}
/*
  for ( var CHARCODE = 0; CHARCODE < 256; CHARCODE++ ) {
  new TestCase( SECTION,
  "unescape( %u"+ ToUnicodeString(CHARCODE)+" )",
  String.fromCharCode(CHARCODE),
  unescape( "%u" + ToUnicodeString(CHARCODE) )  );
  }
  for ( var CHARCODE = 256; CHARCODE < 65536; CHARCODE+= 333 ) {
  new TestCase( SECTION,
  "unescape( %u"+ ToUnicodeString(CHARCODE)+" )",
  String.fromCharCode(CHARCODE),
  unescape( "%u" + ToUnicodeString(CHARCODE) )  );
  }
*/

test();

function ToUnicodeString( n ) {
  var string = ToHexString(n);

  for ( var PAD = (4 - string.length ); PAD > 0; PAD-- ) {
    string = "0" + string;
  }

  return string;
}
function ToHexString( n ) {
  var hex = new Array();

  for ( var mag = 1; Math.pow(16,mag) <= n ; mag++ ) {
    ;
  }

  for ( index = 0, mag -= 1; mag > 0; index++, mag-- ) {
    hex[index] = Math.floor( n / Math.pow(16,mag) );
    n -= Math.pow(16,mag) * Math.floor( n/Math.pow(16,mag) );
  }

  hex[hex.length] = n % 16;

  var string ="";

  for ( var index = 0 ; index < hex.length ; index++ ) {
    switch ( hex[index] ) {
    case 10:
      string += "A";
      break;
    case 11:
      string += "B";
      break;
    case 12:
      string += "C";
      break;
    case 13:
      string += "D";
      break;
    case 14:
      string += "E";
      break;
    case 15:
      string += "F";
      break;
    default:
      string += hex[index];
    }
  }

  if ( string.length == 1 ) {
    string = "0" + string;
  }
  return string;
}
