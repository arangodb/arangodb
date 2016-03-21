// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When Date is called as part of a new expression it is
    a constructor: it initializes the newly created object
es5id: 15.9.3.1_A1_T6
description: 7 arguments, (year, month, date, hours, minutes, seconds, ms)
---*/

if (typeof new Date(1899, 11, 31, 23, 59, 59, 999) !== "object") {
  $ERROR("#1.1: typeof new Date(1899, 11, 31, 23, 59, 59, 999) should be 'object'");
}

if (new Date(1899, 11, 31, 23, 59, 59, 999) === undefined) {
  $ERROR("#1.2: new Date(1899, 11, 31, 23, 59, 59, 999) should not be undefined");
}

var x13 = new Date(1899, 11, 31, 23, 59, 59, 999);
if(typeof x13 !== "object"){
  $ERROR("#1.3: typeof new Date(1899, 11, 31, 23, 59, 59, 999) should be 'object'");
}

var x14 = new Date(1899, 11, 31, 23, 59, 59, 999);
if(x14 === undefined){
  $ERROR("#1.4: new Date(1899, 11, 31, 23, 59, 59, 999) should not be undefined");
}

if (typeof new Date(1899, 12, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#2.1: typeof new Date(1899, 12, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(1899, 12, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#2.2: new Date(1899, 12, 1, 0, 0, 0, 0) should not be undefined");
}

var x23 = new Date(1899, 12, 1, 0, 0, 0, 0);
if(typeof x23 !== "object"){
  $ERROR("#2.3: typeof new Date(1899, 12, 1, 0, 0, 0, 0) should be 'object'");
}

var x24 = new Date(1899, 12, 1, 0, 0, 0, 0);
if(x24 === undefined){
  $ERROR("#2.4: new Date(1899, 12, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(1900, 0, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#3.1: typeof new Date(1900, 0, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(1900, 0, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#3.2: new Date(1900, 0, 1, 0, 0, 0, 0) should not be undefined");
}

var x33 = new Date(1900, 0, 1, 0, 0, 0, 0);
if(typeof x33 !== "object"){
  $ERROR("#3.3: typeof new Date(1900, 0, 1, 0, 0, 0, 0) should be 'object'");
}

var x34 = new Date(1900, 0, 1, 0, 0, 0, 0);
if(x34 === undefined){
  $ERROR("#3.4: new Date(1900, 0, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(1969, 11, 31, 23, 59, 59, 999) !== "object") {
  $ERROR("#4.1: typeof new Date(1969, 11, 31, 23, 59, 59, 999) should be 'object'");
}

if (new Date(1969, 11, 31, 23, 59, 59, 999) === undefined) {
  $ERROR("#4.2: new Date(1969, 11, 31, 23, 59, 59, 999) should not be undefined");
}

var x43 = new Date(1969, 11, 31, 23, 59, 59, 999);
if(typeof x43 !== "object"){
  $ERROR("#4.3: typeof new Date(1969, 11, 31, 23, 59, 59, 999) should be 'object'");
}

var x44 = new Date(1969, 11, 31, 23, 59, 59, 999);
if(x44 === undefined){
  $ERROR("#4.4: new Date(1969, 11, 31, 23, 59, 59, 999) should not be undefined");
}

if (typeof new Date(1969, 12, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#5.1: typeof new Date(1969, 12, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(1969, 12, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#5.2: new Date(1969, 12, 1, 0, 0, 0, 0) should not be undefined");
}

var x53 = new Date(1969, 12, 1, 0, 0, 0, 0);
if(typeof x53 !== "object"){
  $ERROR("#5.3: typeof new Date(1969, 12, 1, 0, 0, 0, 0) should be 'object'");
}

var x54 = new Date(1969, 12, 1, 0, 0, 0, 0);
if(x54 === undefined){
  $ERROR("#5.4: new Date(1969, 12, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(1970, 0, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#6.1: typeof new Date(1970, 0, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(1970, 0, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#6.2: new Date(1970, 0, 1, 0, 0, 0, 0) should not be undefined");
}

var x63 = new Date(1970, 0, 1, 0, 0, 0, 0);
if(typeof x63 !== "object"){
  $ERROR("#6.3: typeof new Date(1970, 0, 1, 0, 0, 0, 0) should be 'object'");
}

var x64 = new Date(1970, 0, 1, 0, 0, 0, 0);
if(x64 === undefined){
  $ERROR("#6.4: new Date(1970, 0, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(1999, 11, 31, 23, 59, 59, 999) !== "object") {
  $ERROR("#7.1: typeof new Date(1999, 11, 31, 23, 59, 59, 999) should be 'object'");
}

if (new Date(1999, 11, 31, 23, 59, 59, 999) === undefined) {
  $ERROR("#7.2: new Date(1999, 11, 31, 23, 59, 59, 999) should not be undefined");
}

var x73 = new Date(1999, 11, 31, 23, 59, 59, 999);
if(typeof x73 !== "object"){
  $ERROR("#7.3: typeof new Date(1999, 11, 31, 23, 59, 59, 999) should be 'object'");
}

var x74 = new Date(1999, 11, 31, 23, 59, 59, 999);
if(x74 === undefined){
  $ERROR("#7.4: new Date(1999, 11, 31, 23, 59, 59, 999) should not be undefined");
}

if (typeof new Date(1999, 12, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#8.1: typeof new Date(1999, 12, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(1999, 12, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#8.2: new Date(1999, 12, 1, 0, 0, 0, 0) should not be undefined");
}

var x83 = new Date(1999, 12, 1, 0, 0, 0, 0);
if(typeof x83 !== "object"){
  $ERROR("#8.3: typeof new Date(1999, 12, 1, 0, 0, 0, 0) should be 'object'");
}

var x84 = new Date(1999, 12, 1, 0, 0, 0, 0);
if(x84 === undefined){
  $ERROR("#8.4: new Date(1999, 12, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(2000, 0, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#9.1: typeof new Date(2000, 0, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(2000, 0, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#9.2: new Date(2000, 0, 1, 0, 0, 0, 0) should not be undefined");
}

var x93 = new Date(2000, 0, 1, 0, 0, 0, 0);
if(typeof x93 !== "object"){
  $ERROR("#9.3: typeof new Date(2000, 0, 1, 0, 0, 0, 0) should be 'object'");
}

var x94 = new Date(2000, 0, 1, 0, 0, 0, 0);
if(x94 === undefined){
  $ERROR("#9.4: new Date(2000, 0, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(2099, 11, 31, 23, 59, 59, 999) !== "object") {
  $ERROR("#10.1: typeof new Date(2099, 11, 31, 23, 59, 59, 999) should be 'object'");
}

if (new Date(2099, 11, 31, 23, 59, 59, 999) === undefined) {
  $ERROR("#10.2: new Date(2099, 11, 31, 23, 59, 59, 999) should not be undefined");
}

var x103 = new Date(2099, 11, 31, 23, 59, 59, 999);
if(typeof x103 !== "object"){
  $ERROR("#10.3: typeof new Date(2099, 11, 31, 23, 59, 59, 999) should be 'object'");
}

var x104 = new Date(2099, 11, 31, 23, 59, 59, 999);
if(x104 === undefined){
  $ERROR("#10.4: new Date(2099, 11, 31, 23, 59, 59, 999) should not be undefined");
}

if (typeof new Date(2099, 12, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#11.1: typeof new Date(2099, 12, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(2099, 12, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#11.2: new Date(2099, 12, 1, 0, 0, 0, 0) should not be undefined");
}

var x113 = new Date(2099, 12, 1, 0, 0, 0, 0);
if(typeof x113 !== "object"){
  $ERROR("#11.3: typeof new Date(2099, 12, 1, 0, 0, 0, 0) should be 'object'");
}

var x114 = new Date(2099, 12, 1, 0, 0, 0, 0);
if(x114 === undefined){
  $ERROR("#11.4: new Date(2099, 12, 1, 0, 0, 0, 0) should not be undefined");
}

if (typeof new Date(2100, 0, 1, 0, 0, 0, 0) !== "object") {
  $ERROR("#12.1: typeof new Date(2100, 0, 1, 0, 0, 0, 0) should be 'object'");
}

if (new Date(2100, 0, 1, 0, 0, 0, 0) === undefined) {
  $ERROR("#12.2: new Date(2100, 0, 1, 0, 0, 0, 0) should not be undefined");
}

var x123 = new Date(2100, 0, 1, 0, 0, 0, 0);
if(typeof x123 !== "object"){
  $ERROR("#12.3: typeof new Date(2100, 0, 1, 0, 0, 0, 0) should be 'object'");
}

var x124 = new Date(2100, 0, 1, 0, 0, 0, 0);
if(x124 === undefined){
  $ERROR("#12.4: new Date(2100, 0, 1, 0, 0, 0, 0) should not be undefined");
}
