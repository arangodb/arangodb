// Copyright (c) 2014 Ecma International.  All rights reserved.
// See LICENSE or https://github.com/tc39/test262/blob/master/LICENSE

/*---
info: Array.prototype.concat uses [[Get]] on 'length' to determine array length
es5id: 15.4.4.4_A3_T2
description: >
  checking whether non-ownProperties are seen, copied by Array.prototype.concat: Array.prototype[1]
---*/

var a = [0];

if (a.length !== 1) {
  $ERROR("expected a.length === 1, actually " + a.length);
}

a.length = 3;

if (a[1] !== undefined) {
  $ERROR("expected a[1] === undefined, actually " + a[1]);
}
if (a[2] !== undefined) {
  $ERROR("expected a[2] === undefined, actually " + a[2]);
}

Array.prototype[2] = 2;

if (a[1] !== undefined) {
  $ERROR("expected a[1] === undefined, actually " + a[1]);
}
if (a[2] !== 2) {
  $ERROR("expected a[2] === 2, actually " + a[2]);
}

if (a.hasOwnProperty('1') !== false) {
  $ERROR("a.hasOwnProperty('1') === false, actually " + a.hasOwnProperty('1'));
}
if (a.hasOwnProperty('2') !== false) {
  $ERROR("a.hasOwnProperty('2') === false, actually " + a.hasOwnProperty('2'));
}

var b = a.concat();

if (b.length !== 3) {
  $ERROR("expected b.length === 3, actually " + b.length);
}

if (b[0] !== 0) {
  $ERROR("expected b[0] === 0, actually " + b[0]);
}
if (b[1] !== undefined) {
  $ERROR("expected b[1] === undefined, actually " + b[1]);
}
if (b[2] !== 2) {
  $ERROR("expected b[2] === 2, actually " + b[2]);
}

if (b.hasOwnProperty('1') !== false) {
  $ERROR("expected b.hasOwnProperty('1') === false, actually " + b.hasOwnProperty('1'));
}
if (b.hasOwnProperty('2') !== true) {
  $ERROR("expected b.hasOwnProperty('2') === true, actually " + b.hasOwnProperty('2'));
}

