/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Martin Honnen
 *                 Brendan Eich
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

gTestfile = '10.2.1.js';

START("10.2.1 - XML.toXMLString");

var BUGNUMBER = 297025;

printBugNumber(BUGNUMBER);

var n = 1;
var actual;
var expect;


// Semantics

var xml;

// text 10.2.1 - Step 4

printStatus(inSection(n++) + ' testing text');

var text = 'this is text';


printStatus(inSection(n++) + ' testing text with pretty printing');
XML.prettyPrinting = true;
XML.prettyIndent   = 10;
xml = new XML(text);
expect = text;
actual = xml.toXMLString();
TEST(n, expect, actual);

printStatus(inSection(n++) + ' testing text with whitespace with pretty printing');
XML.prettyPrinting = true;
XML.prettyIndent   = 10;
xml = new XML(' \t\r\n' + text + ' \t\r\n');
expect = text;
actual = xml.toXMLString();
TEST(n, expect, actual);

printStatus(inSection(n++) + ' testing text with whitespace without pretty printing');
XML.prettyPrinting = false;
xml = new XML(' \t\r\n' + text + ' \t\r\n');
expect = ' \t\r\n' + text + ' \t\r\n';
actual = xml.toXMLString();
TEST(n, expect, actual);

// EscapeElementValue - 10.2.1 Step 4.a.ii

printStatus(inSection(n++) + ' testing text EscapeElementValue');
var alpha = 'abcdefghijklmnopqrstuvwxyz';
xml = <a>{"< > &"}{alpha}</a>;
xml = xml.text();
expect = '&lt; &gt; &amp;' + alpha
actual = xml.toXMLString();
TEST(n, expect, actual);

// attribute, EscapeAttributeValue - 10.2.1 Step 5

printStatus(inSection(n++) + ' testing attribute EscapeAttributeValue');
xml = <foo/>;
xml.@bar = '"<&\u000A\u000D\u0009' + alpha;
expect = '<foo bar="&quot;&lt;&amp;&#xA;&#xD;&#x9;' + alpha + '"/>';
actual = xml.toXMLString();
TEST(n, expect, actual);

// Comment - 10.2.1 Step 6

printStatus(inSection(n++) + ' testing Comment');
XML.ignoreComments = false;
xml = <!-- Comment -->;
expect = '<!-- Comment -->';
actual = xml.toXMLString();
TEST(n, expect, actual);


// Processing Instruction - 10.2.1 Step 7

printStatus(inSection(n++) + ' testing Processing Instruction');
XML.ignoreProcessingInstructions = false;
xml = <?pi value?>;
expect = '<?pi value?>';
actual = xml.toXMLString();
TEST(n, expect, actual);

// 10.2.1 Step 8+

// From Martin and Brendan
var link;
var xlinkNamespace;

printStatus(inSection(n++));
expect = '<link xlink:type="simple" xmlns:xlink="http://www.w3.org/1999/xlink"/>';

link = <link type="simple" />;
xlinkNamespace = new Namespace('xlink', 'http://www.w3.org/1999/xlink');
link.addNamespace(xlinkNamespace);
printStatus('In scope namespace: ' + link.inScopeNamespaces());
printStatus('XML markup: ' + link.toXMLString());
link.@type.setName(new QName(xlinkNamespace, 'type'));
printStatus('name(): ' + link.@*::*[0].name());
actual = link.toXMLString();
printStatus(actual);

TEST(n, expect, actual);

printStatus(inSection(n++));
link = <link type="simple" />;
xlinkNamespace = new Namespace('xlink', 'http://www.w3.org/1999/xlink');
printStatus('XML markup: ' + link.toXMLString());
link.@type.setNamespace(xlinkNamespace);
printStatus('name(): ' + link.@*::*[0].name());
actual = link.toXMLString();
printStatus(actual);

TEST(n, expect, actual);

END();
