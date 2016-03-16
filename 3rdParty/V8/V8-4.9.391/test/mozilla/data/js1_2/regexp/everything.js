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

gTestfile = 'everything.js';

/**
   Filename:     everything.js
   Description:  'Tests regular expressions'

   Author:       Nick Lerissa
   Date:         March 24, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE   = 'RegExp';

writeHeaderToLog('Executing script: everything.js');
writeHeaderToLog( SECTION + " "+ TITLE);


// 'Sally and Fred are sure to come.'.match(/^[a-z\s]*/i)
new TestCase ( SECTION, "'Sally and Fred are sure to come'.match(/^[a-z\\s]*/i)",
	       String(["Sally and Fred are sure to come"]), String('Sally and Fred are sure to come'.match(/^[a-z\s]*/i)));

// 'test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))
new TestCase ( SECTION, "'test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))",
	       String(["test123W+xyz","xyz"]), String('test123W+xyz'.match(new RegExp('^[a-z]*[0-9]+[A-Z]?.(123|xyz)$'))));

// 'number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)
new TestCase ( SECTION, "'number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)",
	       String(["12365 number two 9898","12365","9898"]), String('number one 12365 number two 9898'.match(/(\d+)\D+(\d+)/)));

var simpleSentence = /(\s?[^\!\?\.]+[\!\?\.])+/;
// 'See Spot run.'.match(simpleSentence)
new TestCase ( SECTION, "'See Spot run.'.match(simpleSentence)",
	       String(["See Spot run.","See Spot run."]), String('See Spot run.'.match(simpleSentence)));

// 'I like it. What's up? I said NO!'.match(simpleSentence)
new TestCase ( SECTION, "'I like it. What's up? I said NO!'.match(simpleSentence)",
	       String(["I like it. What's up? I said NO!",' I said NO!']), String('I like it. What\'s up? I said NO!'.match(simpleSentence)));

// 'the quick brown fox jumped over the lazy dogs'.match(/((\w+)\s*)+/)
new TestCase ( SECTION, "'the quick brown fox jumped over the lazy dogs'.match(/((\\w+)\\s*)+/)",
	       String(['the quick brown fox jumped over the lazy dogs','dogs','dogs']),String('the quick brown fox jumped over the lazy dogs'.match(/((\w+)\s*)+/)));

test();
