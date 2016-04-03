// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function ToInteger(p) {
  x = Number(p);

  if(isNaN(x)){
    return +0;
  }
  
  if((x === +0) 
  || (x === -0) 
  || (x === Number.POSITIVE_INFINITY) 
  || (x === Number.NEGATIVE_INFINITY)){
     return x;
  }

  var sign = ( x < 0 ) ? -1 : 1;

  return (sign*Math.floor(Math.abs(x)));
}
