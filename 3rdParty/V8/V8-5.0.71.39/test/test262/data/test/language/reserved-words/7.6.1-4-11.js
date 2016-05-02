// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-4-11
description: >
    Allow reserved words as property names by set function within an
    object, verified with hasOwnProperty: enum, extends, super
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set enum(value){
                test0 = value;
            },
            get enum(){
                return test0;
            },
            set extends(value){
                test1 = value;
            },
            get extends(){
                return test1;
            },
            set super(value){
                test2 = value;
            }, 
            get super(){
                return test2;
            }
        };      
        var arr = [
            'enum',
            'extends',
            'super'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
