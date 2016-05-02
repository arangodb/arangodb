// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

var prec;
function isEqual(num1, num2)
{
        if ((num1 === Infinity)&&(num2 === Infinity))
        {
                return(true);
        }
        if ((num1 === -Infinity)&&(num2 === -Infinity))
        {
                return(true);
        }
        prec = getPrecision(Math.min(Math.abs(num1), Math.abs(num2)));  
        return(Math.abs(num1 - num2) <= prec);
        //return(num1 === num2);
}

