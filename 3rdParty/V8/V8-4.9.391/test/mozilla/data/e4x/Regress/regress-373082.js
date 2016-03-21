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

gTestfile = 'regress-373082.js';

var BUGNUMBER = 373082;
var summary = 'Simpler sharing of XML and XMLList functions';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var l;

expect = '<a/>';
l = <><a/></>;
actual = l.toXMLString();
TEST(1, expect, actual);

expect = '<b/>';
l.setName('b');
actual = l.toXMLString();
TEST(2, expect, actual);

expect = '<c/>';
XMLList.prototype.function::setName.call(l, 'c');
actual = l.toXMLString();
TEST(3, expect, actual);

expect = 't';
l = <><a>text</a></>;
actual = l.charAt(0);
TEST(4, expect, actual);

expect = 'TypeError: String.prototype.toString called on incompatible XML';

try
{
    delete XML.prototype.function::toString;
    delete Object.prototype.toString;
    actual = <a>TEXT</a>.toString();
}
catch(ex)
{
    actual = ex + '';
}
TEST(7, expect, actual);

expect = 'TypeError: String.prototype.toString called on incompatible XML';
try
{
    var x = <a><name/></a>;
    x.(name == "Foo");
    print(x.function::name());
}
catch(ex)
{
    actual = ex + '';
}
TEST(8, expect, actual);

try
{
    x = <a><name/></a>;
    x.(name == "Foo");
    print(x.name());
}
catch(ex)
{
    actual = ex + '';
}
TEST(9, expect, actual);

END();
