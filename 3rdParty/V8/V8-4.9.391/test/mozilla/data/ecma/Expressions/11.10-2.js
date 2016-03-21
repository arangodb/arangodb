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

gTestfile = '11.10-2.js';

/**
   File Name:          11.10-2.js
   ECMA Section:       11.10-2 Binary Bitwise Operators:  |
   Description:
   Semantics

   The production A : A @ B, where @ is one of the bitwise operators in the
   productions &, ^, | , is evaluated as follows:

   1.  Evaluate A.
   2.  Call GetValue(Result(1)).
   3.  Evaluate B.
   4.  Call GetValue(Result(3)).
   5.  Call ToInt32(Result(2)).
   6.  Call ToInt32(Result(4)).
   7.  Apply the bitwise operator @ to Result(5) and Result(6). The result is
   a signed 32 bit integer.
   8.  Return Result(7).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.10-2";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Binary Bitwise Operators:  |");

var shiftexp = 0;
var addexp = 0;

for ( shiftpow = 0; shiftpow < 33; shiftpow++ ) {
  shiftexp += Math.pow( 2, shiftpow );

  for ( addpow = 0; addpow < 33; addpow++ ) {
    addexp += Math.pow(2, addpow);

    new TestCase( SECTION,
		  shiftexp + " | " + addexp,
		  Or( shiftexp, addexp ),
		  shiftexp | addexp );
  }
}

test();

function ToInteger( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( n != n ) {
    return 0;
  }
  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY ) {
    return n;
  }
  return ( sign * Math.floor(Math.abs(n)) );
}
function ToInt32( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }

  n = (sign * Math.floor( Math.abs(n) )) % Math.pow(2,32);
  n = ( n >= Math.pow(2,31) ) ? n - Math.pow(2,32) : n;

  return ( n );
}
function ToUint32( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }
  n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

  if ( n < 0 ){
    n += Math.pow(2,32);
  }

  return ( n );
}
function ToUint16( n ) {
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }

  n = ( sign * Math.floor( Math.abs(n) ) ) % Math.pow(2,16);

  if (n <0) {
    n += Math.pow(2,16);
  }

  return ( n );
}
function Mask( b, n ) {
  b = ToUint32BitString( b );
  b = b.substring( b.length - n );
  b = ToUint32Decimal( b );
  return ( b );
}
function ToUint32BitString( n ) {
  var b = "";
  for ( p = 31; p >=0; p-- ) {
    if ( n >= Math.pow(2,p) ) {
      b += "1";
      n -= Math.pow(2,p);
    } else {
      b += "0";
    }
  }
  return b;
}
function ToInt32BitString( n ) {
  var b = "";
  var sign = ( n < 0 ) ? -1 : 1;

  b += ( sign == 1 ) ? "0" : "1";

  for ( p = 30; p >=0; p-- ) {
    if ( (sign == 1 ) ? sign * n >= Math.pow(2,p) : sign * n > Math.pow(2,p) ) {
      b += ( sign == 1 ) ? "1" : "0";
      n -= sign * Math.pow( 2, p );
    } else {
      b += ( sign == 1 ) ? "0" : "1";
    }
  }

  return b;
}
function ToInt32Decimal( bin ) {
  var r = 0;
  var sign;

  if ( Number(bin.charAt(0)) == 0 ) {
    sign = 1;
    r = 0;
  } else {
    sign = -1;
    r = -(Math.pow(2,31));
  }

  for ( var j = 0; j < 31; j++ ) {
    r += Math.pow( 2, j ) * Number(bin.charAt(31-j));
  }

  return r;
}
function ToUint32Decimal( bin ) {
  var r = 0;


  for ( l = bin.length; l < 32; l++ ) {
    bin = "0" + bin;
  }

  for ( j = 0; j < 31; j++ ) {
    r += Math.pow( 2, j ) * Number(bin.charAt(31-j));

  }

  return r;
}
function And( s, a ) {
  s = ToInt32( s );
  a = ToInt32( a );

  var bs = ToInt32BitString( s );
  var ba = ToInt32BitString( a );

  var result = "";

  for ( var bit = 0; bit < bs.length; bit++ ) {
    if ( bs.charAt(bit) == "1" && ba.charAt(bit) == "1" ) {
      result += "1";
    } else {
      result += "0";
    }
  }
  return ToInt32Decimal(result);
}
function Xor( s, a ) {
  s = ToInt32( s );
  a = ToInt32( a );

  var bs = ToInt32BitString( s );
  var ba = ToInt32BitString( a );

  var result = "";

  for ( var bit = 0; bit < bs.length; bit++ ) {
    if ( (bs.charAt(bit) == "1" && ba.charAt(bit) == "0") ||
	 (bs.charAt(bit) == "0" && ba.charAt(bit) == "1")
      ) {
      result += "1";
    } else {
      result += "0";
    }
  }

  return ToInt32Decimal(result);
}
function Or( s, a ) {
  s = ToInt32( s );
  a = ToInt32( a );

  var bs = ToInt32BitString( s );
  var ba = ToInt32BitString( a );

  var result = "";

  for ( var bit = 0; bit < bs.length; bit++ ) {
    if ( bs.charAt(bit) == "1" || ba.charAt(bit) == "1" ) {
      result += "1";
    } else {
      result += "0";
    }
  }

  return ToInt32Decimal(result);
}
