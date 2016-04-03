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

gTestfile = 'Function_object.js';

/**
   Filename:     Function_object.js
   Description:  'Testing Function objects'

   Author:       Nick Lerissa
   Date:         April 17, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'functions: Function_object';

writeHeaderToLog('Executing script: Function_object.js');
writeHeaderToLog( SECTION + " "+ TITLE);


function a_test_function(a,b,c)
{
  return a + b + c;
}

f = a_test_function;


new TestCase( SECTION, "f.name",
	      'a_test_function',  f.name);

new TestCase( SECTION, "f.length",
	      3,  f.length);

new TestCase( SECTION, "f.arity",
	      3,  f.arity);

new TestCase( SECTION, "f(2,3,4)",
	      9,  f(2,3,4));

var fnName = (version() == 120) ? '' : 'anonymous';

new TestCase( SECTION, "(new Function()).name",
	      fnName, (new Function()).name);

new TestCase( SECTION, "(new Function()).toString()",
	      'function ' + fnName + '() {\n}',  (new Function()).toString());

test();

