// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-9
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: if, throw, delete
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set if(value){
                test0 = value;
            },
            get if(){
                return test0;
            },
            set throw(value){
                test1 = value;
            },
            get throw(){
                return test1
            },
            set delete(value){
                test2 = value;
            },
            get delete(){
                return test2;
            }
        };      
        var arr = [
            'if', 
            'throw', 
            'delete'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
