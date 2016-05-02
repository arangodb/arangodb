// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Properties of the object being enumerated may be deleted during
    enumeration
es5id: 12.6.4_A7_T2
description: >
    Checking "for (var VariableDeclarationNoIn in Expression)
    Statement" case
---*/

var __obj, __accum;

__obj={aa:1,ba:2,ca:3};

__accum="";

for (var __key in __obj){
	
    erasator_T_1000(__obj,"b");
  
	__accum+=(__key+__obj[__key]);
	
}


//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (!((__accum.indexOf("aa1")!==-1)&&(__accum.indexOf("ca3")!==-1))) {
	$ERROR('#1: (__accum.indexOf("aa1")!==-1)&&(__accum.indexOf("ca3")!==-1)');
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (__accum.indexOf("ba2")!==-1) {
	$ERROR('#2: __accum.indexOf("ba2") === -1. Actual:  __accum.indexOf("ba2") ==='+ __accum.indexOf("ba2")  );
}
//
//////////////////////////////////////////////////////////////////////////////


// erasator is the hash map terminator
function erasator_T_1000(hash_map, charactr){
	for (var key in hash_map){
		if (key.indexOf(charactr)===0) {
			delete hash_map[key];
		};
	}
}
