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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Marco Fabbri
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

var gTestfile = 'regress-452008.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 452008;
var summary = 'Bad math with JIT';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);
 
// regression test for Bug 452008 - TM: SRP in Clipperz crypto library fails when JIT (TraceMonkey) is enabled. 

  var x = [9385, 32112, 25383, 16317, 30138, 14565, 17812, 24500, 2719, 30174, 3546, 9096, 15352, 19120, 20648, 14334, 7426, 0, 0, 0];
  var n = [27875, 25925, 30422, 12227, 27798, 32170, 10873, 21748, 30629, 26296, 20697, 5125, 4815, 2221, 14392, 23369, 5560, 2, 0, 0];
  var np = 18229;
  var expected = [18770, 31456, 17999, 32635, 27508, 29131, 2856, 16233, 5439, 27580, 7093, 18192, 30804, 5472, 8529, 28649, 14852, 0, 0, 0];

//globals
  bpe=0;         //bits stored per array element
  mask=0;        //AND this with an array element to chop it down to bpe bits

//initialize the global variables
  for (bpe=0; (1<<(bpe+1)) > (1<<bpe); bpe++);  //bpe=number of bits in the mantissa on this platform
  bpe>>=1;                   //bpe=number of bits in one element of the array representing the bigInt
  mask=(1<<bpe)-1;           //AND the mask with an integer to get its bpe least significant bits


//the following global variables are scratchpad memory to
//reduce dynamic memory allocation in the inner loop
  sa = new Array(0); //used in mont_()

//do x=y on bigInts x and y.  x must be an array at least as big as y (not counting the leading zeros in y).
  function copy_(x,y) {
    var i;
    var k=x.length<y.length ? x.length : y.length;
    for (i=0;i<k;i++)
      x[i]=y[i];
    for (i=k;i<x.length;i++)
      x[i]=0;
  }

//do x=y on bigInt x and integer y.
  function copyInt_(x,n) {
    var i,c;
    for (c=n,i=0;i<x.length;i++) {
      x[i]=c & mask;
      c>>=bpe;
    }
  }

//is x > y? (x and y both nonnegative)
  function greater(x,y) {
    var i;
    var k=(x.length<y.length) ? x.length : y.length;

    for (i=x.length;i<y.length;i++)
      if (y[i])
        return 0;  //y has more digits

    for (i=y.length;i<x.length;i++)
      if (x[i])
        return 1;  //x has more digits

    for (i=k-1;i>=0;i--)
      if (x[i]>y[i])
        return 1;
      else if (x[i]<y[i])
        return 0;
    return 0;
  }


//do x=x*y*Ri mod n for bigInts x,y,n,
//  where Ri = 2**(-kn*bpe) mod n, and kn is the
//  number of elements in the n array, not
//  counting leading zeros.
//x must be large enough to hold the answer.
//It's OK if x and y are the same variable.
//must have:
//  x,y < n
//  n is odd
//  np = -(n^(-1)) mod radix
  function mont_(x,y,n,np) {
    var i,j,c,ui,t;
    var kn=n.length;
    var ky=y.length;

    if (sa.length!=kn)
      sa=new Array(kn);

    for (;kn>0 && n[kn-1]==0;kn--); //ignore leading zeros of n
    for (;ky>0 && y[ky-1]==0;ky--); //ignore leading zeros of y

    copyInt_(sa,0);

    //the following loop consumes 95% of the runtime for randTruePrime_() and powMod_() for large keys
    for (i=0; i<kn; i++) {
      t=sa[0]+x[i]*y[0];
      ui=((t & mask) * np) & mask;  //the inner "& mask" is needed on Macintosh MSIE, but not windows MSIE
      c=(t+ui*n[0]) >> bpe;
      t=x[i];

      //do sa=(sa+x[i]*y+ui*n)/b   where b=2**bpe
      for (j=1;j<ky;j++) {
        c+=sa[j]+t*y[j]+ui*n[j];
        sa[j-1]=c & mask;
        c>>=bpe;
      }
      for (;j<kn;j++) {
        c+=sa[j]+ui*n[j];
        sa[j-1]=c & mask;
        c>>=bpe;
      }
      sa[j-1]=c & mask;
    }

    if (!greater(n,sa))
      sub_(sa,n);
    copy_(x,sa);
  }

  mont_(x, x, n, np);

  var passed = expected.length == x.length;
  for (var i = 0; i < expected.length; i++) {
    if (passed)
      passed = expected[i] == x[i];
  }
  print(passed);

  jit(false);

  expect = true;
  actual = passed;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
