// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-10
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: in, try, class
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set in(value){
                test0 = value;
            },
            get in(){
                return test0;
            },
            set try(value){
                test1 = value;
            },
            get try(){
                return test1
            },
            set class(value){
                test2 = value;
            },
            get class(){
                return test2;
            }
        };      
        var arr = [
            'in', 
            'try',
            'class'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
