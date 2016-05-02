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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com, epstein@tellme.com
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

/*
 * Date: 23 May 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 82306
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=82306
 *
 * This test used to crash the JS engine. This was discovered
 * by Mike Epstein <epstein@tellme.com>
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-82306.js';
var BUGNUMBER = 82306;
var summary = "Testing we don't crash on encodeURI()";
var URI = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  URI += '<?xml version="1.0"?>';
  URI += '<zcti application="xxxx_demo">';
  URI += '<pstn_data>';
  URI += '<ani>650-930-xxxx</ani>';
  URI += '<dnis>877-485-xxxx</dnis>';
  URI += '</pstn_data>';
  URI += '<keyvalue key="name" value="xxx"/>';
  URI += '<keyvalue key="phone" value="6509309000"/>';
  URI += '</zcti>';

  // Just testing that we don't crash on this
  encodeURI(URI);

  reportCompare('No Crash', 'No Crash', '');

  exitFunc ('test');
}
