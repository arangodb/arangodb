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

gTestfile = 'regexp-enumerate-001.js';

/**
   File Name:          regexp-enumerate-001.js
   ECMA V2 Section:
   Description:        Regression Test.

   If instance Native Object have properties that are enumerable,
   JavaScript enumerated through the properties twice. This only
   happened if objects had been instantiated, but their properties
   had not been enumerated.  ie, the object inherited properties
   from its prototype that are enumerated.

   In the core JavaScript, this is only a problem with RegExp
   objects, since the inherited properties of most core JavaScript
   objects are not enumerated.

   Author:             christine@netscape.com, pschwartau@netscape.com
   Date:               12 November 1997
   Modified:           14 July 2002
   Reason:             See http://bugzilla.mozilla.org/show_bug.cgi?id=155291
   ECMA-262 Ed.3  Sections 15.10.7.1 through 15.10.7.5
   RegExp properties should be DontEnum
   *
   */
//    onerror = err;

var SECTION = "regexp-enumerate-001";
var VERSION = "ECMA_2";
var TITLE   = "Regression Test for Enumerating Properties";

var BUGNUMBER="339403";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

/*
 *  This test expects RegExp instances to have four enumerated properties:
 *  source, global, ignoreCase, and lastIndex
 *
 *  99.01.25:  now they also have a multiLine instance property.
 *
 */


var r = new RegExp();

var e = new Array();

var t = new TestRegExp();

for ( p in r ) { e[e.length] = { property:p, value:r[p] }; t.addProperty( p, r[p]) };

new TestCase( SECTION,
	      "r = new RegExp(); e = new Array(); "+
	      "for ( p in r ) { e[e.length] = { property:p, value:r[p] }; e.length",
	      0,
	      e.length );

test();

function TestRegExp() {
  this.addProperty = addProperty;
}
function addProperty(name, value) {
  var pass = false;

  if ( eval("this."+name) != void 0 ) {
    pass = true;
  } else {
    eval( "this."+ name+" = "+ false );
  }

  new TestCase( SECTION,
		"Property: " + name +" already enumerated?",
		false,
		pass );

  if ( gTestcases[ gTestcases.length-1].passed == false ) {
    gTestcases[gTestcases.length-1].reason = "property already enumerated";

  }

}
