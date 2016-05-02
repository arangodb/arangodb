/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

gTestfile = 'regress-376773.js';

var BUGNUMBER = 376773;
var summary = 'xmlsimple.stringmethod === xmlsimple.function::stringmethod';
var actual = '';
var expect = '';
var actualcall = '';
var expectcall = '';

printBugNumber(BUGNUMBER);
START(summary);

var nTest = 0;
var xml = <a>TEXT</a>;

// --------------------------------------------------------------

String.prototype.orig_toString = String.prototype.toString;
String.prototype.toString = function() {
    actualcall = 'String.prototype.toString called';
    return this.orig_toString();
};

expect = 'TEXT';
expectcall = 'String.prototype.toString not called';

actualcall = expectcall;
actual = xml.toString();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::toString();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::toString.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toString = String.prototype.orig_toString;
delete String.prototype.orig_toString;

// --------------------------------------------------------------

String.prototype.orig_toSource = String.prototype.toSource;
String.prototype.toSource = function() {
    actualcall = 'String.prototype.toSource called';
    return this.orig_toSource();
};

expect = '<a>TEXT</a>';
expectcall = 'String.prototype.toSource not called';

actualcall = expectcall;
actual = xml.toSource();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::toSource();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::toSource.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toSource = String.prototype.orig_toSource;
delete String.prototype.orig_toSource;

// --------------------------------------------------------------

String.prototype.orig_valueOf = String.prototype.valueOf;
String.prototype.valueOf = function() {
    actualcall = 'String.prototype.valueOf called';
    return this.orig_valueOf();
};

expect = 'TEXT';
expectcall = 'String.prototype.valueOf not called';

actualcall = expectcall;
actual = xml.valueOf();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::valueOf();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::valueOf.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.valueOf = String.prototype.orig_valueOf;
delete String.prototype.orig_valueOf;

// --------------------------------------------------------------

String.prototype.orig_charAt = String.prototype.charAt;
String.prototype.charAt = function(pos) {
    actualcall = 'String.prototype.charAt called';
    return this.orig_charAt(pos);
};

expect = 'T';
expectcall = 'String.prototype.charAt called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.charAt(0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::charAt(0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::charAt.call(xml, 0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.charAt = String.prototype.orig_charAt;
delete String.prototype.orig_charAt;

// --------------------------------------------------------------

String.prototype.orig_charCodeAt = String.prototype.charCodeAt;
String.prototype.charCodeAt = function(pos) {
    actualcall = 'String.prototype.charCodeAt called';
    return this.orig_charCodeAt(pos);
};

expect = 84;
expectcall = 'String.prototype.charCodeAt called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.charCodeAt(0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::charCodeAt(0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::charCodeAt.call(xml, 0);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.charCodeAt = String.prototype.orig_charCodeAt;
delete String.prototype.orig_charCodeAt;

// --------------------------------------------------------------

String.prototype.orig_concat = String.prototype.concat;
String.prototype.concat = function(string1) {
    actualcall = 'String.prototype.concat called';
    return this.orig_concat(string1);
};

expect = 'TEXTtext';
expectcall = 'String.prototype.concat called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.concat(<b>text</b>);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::concat(<b>text</b>);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::concat.call(xml, <b>text</b>);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.concat = String.prototype.orig_concat;
delete String.prototype.orig_concat;

// --------------------------------------------------------------

String.prototype.orig_indexOf = String.prototype.indexOf;
String.prototype.indexOf = function(searchString, position) {
    actualcall = 'String.prototype.indexOf called';
    return this.orig_indexOf(searchString, position);
};

expect = 0;
expectcall = 'String.prototype.indexOf called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.indexOf('T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::indexOf('T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::indexOf.call(xml, 'T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.indexOf = String.prototype.orig_indexOf;
delete String.prototype.orig_indexOf;

// --------------------------------------------------------------

String.prototype.orig_lastIndexOf = String.prototype.lastIndexOf;
String.prototype.lastIndexOf = function(searchString, position) {
    actualcall = 'String.prototype.lastIndexOf called';
    return this.orig_lastIndexOf(searchString, position);
};

expect = 3;
expectcall = 'String.prototype.lastIndexOf called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.lastIndexOf('T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::lastIndexOf('T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::lastIndexOf.call(xml, 'T');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.lastIndexOf = String.prototype.orig_lastIndexOf;
delete String.prototype.orig_lastIndexOf;

// --------------------------------------------------------------

String.prototype.orig_localeCompare = String.prototype.localeCompare;
String.prototype.localeCompare = function(that) {
    actualcall = 'String.prototype.localeCompare called';
    return this.orig_localeCompare(that);
};

expect = 0;
expectcall = 'String.prototype.localeCompare called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.localeCompare(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::localeCompare(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::localeCompare.call(xml, xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.localeCompare = String.prototype.orig_localeCompare;
delete String.prototype.orig_localeCompare;

// --------------------------------------------------------------

String.prototype.orig_match = String.prototype.match;
String.prototype.match = function(regexp) {
    actualcall = 'String.prototype.match called';
    return this.orig_match(regexp);
};

expect = ['TEXT'];
expectcall = 'String.prototype.match called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.match(/TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::match(/TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.function::match.call(xml, /TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.match = String.prototype.orig_match;
delete String.prototype.orig_match;

// --------------------------------------------------------------

String.prototype.orig_replace = String.prototype.replace;
String.prototype.replace = function(searchValue, replaceValue) {
    actualcall = 'String.prototype.replace called';
    return this.orig_replace(searchValue, replaceValue);
};

expect = 'TEXT';
expectcall = 'String.prototype.replace not called';

actualcall = expectcall;
actual = xml.replace(/EXT/, 'ext');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::replace(/EXT/, 'ext');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall;
actual = xml.function::replace.call(xml, /EXT/, 'ext');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.replace = String.prototype.orig_replace;
delete String.prototype.orig_replace;

// --------------------------------------------------------------

String.prototype.orig_search = String.prototype.search;
String.prototype.search = function(regexp) {
    actualcall = 'String.prototype.search called';
    return this.orig_search(regexp);
};

expect = 0;
expectcall = 'String.prototype.search called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.search(/TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.search(/called/, 'not called');
actual = xml.function::search(/TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.search(/called/, 'not called');
actual = xml.function::search.call(xml, /TEXT/);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.search = String.prototype.orig_search;
delete String.prototype.orig_search;

// --------------------------------------------------------------

String.prototype.orig_slice = String.prototype.slice;
String.prototype.slice = function(start, end) {
    actualcall = 'String.prototype.slice called';
    return this.orig_slice(start, end);
};

expect = '';
expectcall = 'String.prototype.slice called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.slice(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.slice(/called/, 'not called');
actual = xml.function::slice(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.slice(/called/, 'not called');
actual = xml.function::slice.call(xml, 1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.slice = String.prototype.orig_slice;
delete String.prototype.orig_slice;

// --------------------------------------------------------------

String.prototype.orig_split = String.prototype.split;
String.prototype.split = function(separator, limit) {
    actualcall = 'String.prototype.split called';
    return this.orig_split(separator, limit);
};

expect = ['T', 'E', 'X', 'T'];
expectcall = 'String.prototype.split called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.split('');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.split(/called/, 'not called');
actual = xml.function::split('');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.split(/called/, 'not called');
actual = xml.function::split.call(xml, '');
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.split = String.prototype.orig_split;
delete String.prototype.orig_split;

// --------------------------------------------------------------

String.prototype.orig_substr = String.prototype.substr;
String.prototype.substr = function(start, length) {
    actualcall = 'String.prototype.substr called';
    return this.orig_substr(start, length);
};

expect = 'E';
expectcall = 'String.prototype.substr called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.substr(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.substr(/called/, 'not called');
actual = xml.function::substr(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.substr(/called/, 'not called');
actual = xml.function::substr.call(xml, 1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.substr = String.prototype.orig_substr;
delete String.prototype.orig_substr;

// --------------------------------------------------------------

String.prototype.orig_substring = String.prototype.substring;
String.prototype.substring = function(start, end) {
    actualcall = 'String.prototype.substring called';
    return this.orig_substring(start, end);
};

expect = '';
expectcall = 'String.prototype.substring called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.substring(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.substring(/called/, 'not called');
actual = xml.function::substring(1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.substring(/called/, 'not called');
actual = xml.function::substring.call(xml, 1,1);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.substring = String.prototype.orig_substring;
delete String.prototype.orig_substring;

// --------------------------------------------------------------

String.prototype.orig_toLowerCase = String.prototype.toLowerCase;
String.prototype.toLowerCase = function() {
    actualcall = 'String.prototype.toLowerCase called';
    return this.orig_toLowerCase();
};

expect = 'text';
expectcall = 'String.prototype.toLowerCase called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.toLowerCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLowerCase(/called/, 'not called');
actual = xml.function::toLowerCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLowerCase(/called/, 'not called');
actual = xml.function::toLowerCase.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toLowerCase = String.prototype.orig_toLowerCase;
delete String.prototype.orig_toLowerCase;

// --------------------------------------------------------------

String.prototype.orig_toLocaleLowerCase = String.prototype.toLocaleLowerCase;
String.prototype.toLocaleLowerCase = function() {
    actualcall = 'String.prototype.toLocaleLowerCase called';
    return this.orig_toLocaleLowerCase();
};

expect = 'text';
expectcall = 'String.prototype.toLocaleLowerCase called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.toLocaleLowerCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLocaleLowerCase(/called/, 'not called');
actual = xml.function::toLocaleLowerCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLocaleLowerCase(/called/, 'not called');
actual = xml.function::toLocaleLowerCase.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toLocaleLowerCase = String.prototype.orig_toLocaleLowerCase;
delete String.prototype.orig_toLocaleLowerCase;

// --------------------------------------------------------------

String.prototype.orig_toUpperCase = String.prototype.toUpperCase;
String.prototype.toUpperCase = function() {
    actualcall = 'String.prototype.toUpperCase called';
    return this.orig_toUpperCase();
};

expect = 'TEXT';
expectcall = 'String.prototype.toUpperCase called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.toUpperCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toUpperCase(/called/, 'not called');
actual = xml.function::toUpperCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toUpperCase(/called/, 'not called');
actual = xml.function::toUpperCase.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toUpperCase = String.prototype.orig_toUpperCase;
delete String.prototype.orig_toUpperCase;

// --------------------------------------------------------------

String.prototype.orig_toLocaleUpperCase = String.prototype.toLocaleUpperCase;
String.prototype.toLocaleUpperCase = function() {
    actualcall = 'String.prototype.toLocaleUpperCase called';
    return this.orig_toLocaleUpperCase();
};

expect = 'TEXT';
expectcall = 'String.prototype.toLocaleUpperCase called';

actualcall = expectcall.replace(/called/, 'not called');
actual = xml.toLocaleUpperCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLocaleUpperCase(/called/, 'not called');
actual = xml.function::toLocaleUpperCase();
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

actualcall = expectcall.toLocaleUpperCase(/called/, 'not called');
actual = xml.function::toLocaleUpperCase.call(xml);
TEST(++nTest, expectcall + ':' + expect, actualcall + ':' + actual);

String.prototype.toLocaleUpperCase = String.prototype.orig_toLocaleUpperCase;
delete String.prototype.orig_toLocaleUpperCase;

var l = <><a>text</a></>;
expect = 't';
actual = l.function::charAt.call(l, 0);
TEST(++nTest, expect, actual);

expect = 't';
with (l) actual = function::charAt(0);
TEST(++nTest, expect, actual);


END();
