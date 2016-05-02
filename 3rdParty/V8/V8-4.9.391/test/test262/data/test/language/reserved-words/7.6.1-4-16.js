// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-16
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: undefined, NaN, Infinity
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set undefined(value){
                test0 = value;
            },
            get undefined(){
                return test0;
            },
            set NaN(value){
                test1 = value;
            },
            get NaN(){
                return test1;
            },
            set Infinity(value){
                test2 = value;
            },
            get Infinity(){
                return test2;
            }
        };      
        var arr = [
            'undefined',
            'NaN',
            'Infinity'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
