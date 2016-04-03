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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communication Corporation.
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

gTestfile = 'properties-001.js';

/**
 *  File Name:          RegExp/properties-001.js
 *  ECMA Section:       15.7.6.js
 *  Description:        Based on ECMA 2 Draft 7 February 1999
 *
 *  Author:             christine@netscape.com
 *  Date:               19 February 1999
 */
var SECTION = "RegExp/properties-001.js";
var VERSION = "ECMA_2";
var TITLE   = "Properties of RegExp Instances";
var BUGNUMBER ="";

startTest();

AddRegExpCases( new RegExp, "",   false, false, false, 0 );
AddRegExpCases( /.*/,       ".*", false, false, false, 0 );
AddRegExpCases( /[\d]{5}/g, "[\\d]{5}", true, false, false, 0 );
AddRegExpCases( /[\S]?$/i,  "[\\S]?$", false, true, false, 0 );
AddRegExpCases( /^([a-z]*)[^\w\s\f\n\r]+/m,  "^([a-z]*)[^\\w\\s\\f\\n\\r]+", false, false, true, 0 );
AddRegExpCases( /[\D]{1,5}[\ -][\d]/gi,      "[\\D]{1,5}[\\ -][\\d]", true, true, false, 0 );
AddRegExpCases( /[a-zA-Z0-9]*/gm, "[a-zA-Z0-9]*", true, false, true, 0 );
AddRegExpCases( /x|y|z/gim, "x|y|z", true, true, true, 0 );

AddRegExpCases( /\u0051/im, "\\u0051", false, true, true, 0 );
AddRegExpCases( /\x45/gm, "\\x45", true, false, true, 0 );
AddRegExpCases( /\097/gi, "\\097", true, true, false, 0 );

test();

function AddRegExpCases( re, s, g, i, m, l ) {

  AddTestCase( re + ".test == RegExp.prototype.test",
	       true,
	       re.test == RegExp.prototype.test );

  AddTestCase( re + ".toString == RegExp.prototype.toString",
	       true,
	       re.toString == RegExp.prototype.toString );

  AddTestCase( re + ".contructor == RegExp.prototype.constructor",
	       true,
	       re.constructor == RegExp.prototype.constructor );

  AddTestCase( re + ".compile == RegExp.prototype.compile",
	       true,
	       re.compile == RegExp.prototype.compile );

  AddTestCase( re + ".exec == RegExp.prototype.exec",
	       true,
	       re.exec == RegExp.prototype.exec );

  // properties

  AddTestCase( re + ".source",
	       s,
	       re.source );

/*
 * http://bugzilla.mozilla.org/show_bug.cgi?id=225550 changed
 * the behavior of toString() and toSource() on empty regexps.
 * So branch if |s| is the empty string -
 */
  var S = s? s : '(?:)';

  AddTestCase( re + ".toString()",
	       "/" + S +"/" + (g?"g":"") + (i?"i":"") +(m?"m":""),
	       re.toString() );

  AddTestCase( re + ".global",
	       g,
	       re.global );

  AddTestCase( re + ".ignoreCase",
	       i,
	       re.ignoreCase );

  AddTestCase( re + ".multiline",
	       m,
	       re.multiline);

  AddTestCase( re + ".lastIndex",
	       l,
	       re.lastIndex  );
}
