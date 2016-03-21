// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Error.prototype.toString returns an implementation defined string
es5id: 15.11.4.4_A2
description: Checking if call of Error.prototype.toSting() fails
---*/

//////////////////////////////////////////////////////////////////////////////
// CHECK#1
var err1=new Error("Error");
try{
	var toStr=err1.toString();
}
catch(e){
	$ERROR('#1: var err1=new Error("Error"); var toStr=err1.toString(); lead to throwing exception. Exception is '+e);
}
if (toStr===undefined) {
	$ERROR('#2: var err1=new Error("Error"); var toStr=err1.toString(); toStr!==undefined. Actual: '+toStr);
}
//
//////////////////////////////////////////////////////////////////////////////
