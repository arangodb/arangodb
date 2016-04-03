// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-11
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: enum, extends, super
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
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
