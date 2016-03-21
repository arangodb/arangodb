/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

gTestfile = 'general1.js';

/**
   Filename:     general1.js
   Description:  'This tests out some of the functionality on methods on the Array objects'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:push,unshift,shift';

writeHeaderToLog('Executing script: general1.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var array1 = [];

array1.push(123);            //array1 = [123]
array1.push("dog");          //array1 = [123,dog]
array1.push(-99);            //array1 = [123,dog,-99]
array1.push("cat");          //array1 = [123,dog,-99,cat]
new TestCase( SECTION, "array1.pop()", array1.pop(),'cat');
//array1 = [123,dog,-99]
array1.push("mouse");        //array1 = [123,dog,-99,mouse]
new TestCase( SECTION, "array1.shift()", array1.shift(),123);
//array1 = [dog,-99,mouse]
array1.unshift(96);          //array1 = [96,dog,-99,mouse]
new TestCase( SECTION, "state of array", String([96,"dog",-99,"mouse"]), String(array1));
new TestCase( SECTION, "array1.length", array1.length,4);
array1.shift();              //array1 = [dog,-99,mouse]
array1.shift();              //array1 = [-99,mouse]
array1.shift();              //array1 = [mouse]
new TestCase( SECTION, "array1.shift()", array1.shift(),"mouse");
new TestCase( SECTION, "array1.shift()", "undefined", String(array1.shift()));

test();

