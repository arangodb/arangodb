// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-6
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: continue, for, switch
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set continue(value){
                test0 = value;
            },
            get continue(){
                return test0;
            },
            set for(value){
                test1 = value;
            },
            get for(){
                return test1;
            },
            set switch(value){
                test2 = value;
            },
            get switch(){
                return test2;
            }
        };      
        var arr = [
            'continue', 
            'for',
            'switch'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
