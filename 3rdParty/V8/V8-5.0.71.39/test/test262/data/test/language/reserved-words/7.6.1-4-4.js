// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-4
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: new, var, catch
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set new(value){
                test0 = value;
            },
            get new(){
                return test0;
            },
            set var(value){
                test1 = value;
            },
            get var(){
                return test1;
            },
            set catch(value){
                test2 = value;
            },
            get catch(){
                return test2;
            }
        };      
        var arr = [
            'new', 
            'var', 
            'catch'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
