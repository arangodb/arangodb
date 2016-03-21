// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 Symbol.species is retained on subclassing
es6id: 
author: Sam Mikes
description: Symbol.species is retained on subclassing
includes: 
  - propertyHelper.js
---*/

class MyRegExp extends RegExp {
};

assert.sameValue(MyRegExp[Symbol.species], MyRegExp);

