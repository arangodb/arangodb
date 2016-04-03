// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Two regular expression literals in a program evaluate to
    regular expression objects that never compare as === to each other even
    if the two literals' contents are identical
es5id: 7.8.5_A4.2
description: Check equality two regular expression literals
---*/

//CHECK#1
var regexp1 = /(?:)/;
var regexp2 = /(?:)/;
if (regexp1 === regexp2) {
  $ERROR('#1: var regexp1 = /(?:)/; var regexp2 = /(?:)/; regexp1 !== regexp2');
}
