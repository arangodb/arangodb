// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-2
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: break, case, do
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set break(value){
                test0 = value;        
            },
            get break(){
                return test0;
            },
            set case(value){
                test1 = value;
            },
            get case(){
                return test1;
            },
            set do(value){
                test2 = value;
            },
            get do(){
                return test2;
            }
        };      
        var arr = [
            'break',
            'case',
            'do'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
