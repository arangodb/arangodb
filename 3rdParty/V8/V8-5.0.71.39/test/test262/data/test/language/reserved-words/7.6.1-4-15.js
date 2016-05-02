// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-15
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: package, protected, static
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set package(value){
                test0 = value;
            },
            get package(){
                return test0;
            },
            set protected(value){
                test1 = value;
            },
            get protected(){
                return test1
            },
            set static(value){
                test2 = value;
            },
            get static(){
                return test2;
            }
        };      
        var arr = [
            'package',
            'protected',
            'static'  
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
