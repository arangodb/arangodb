/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Sean McMurray
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

gTestfile = 'regress-312196.js';

var summary = "Extending E4X XML objects with __noSuchMethod__";
var BUGNUMBER = 312196;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

XML.ignoreWhitespace = true;
XML.prettyPrint = false;

function ajaxObj(content){
    var ajax = new XML(content);
    ajax.function::__noSuchMethod__ = autoDispatch;
    return ajax;
}

function autoDispatch(id){
    if (! this.@uri)
        return;
    var request = <request method={id}/>;
    for(var i=0; i<arguments[1].length; i++){
        request += <parameter>{arguments[1][i]}</parameter>;
    }
    var xml = this;
    xml.request = request;
    return(xml.toString());
}

var ws = new ajaxObj('<object uri="http://localhost"/>');

try
{
    actual = ws.function::sample('this', 'is', 'a', 'test');
}
catch(e)
{
    actual = e + '';
}

expect =
(<object uri="http://localhost">
<request method="sample"/>
<parameter>this</parameter>
<parameter>is</parameter>
<parameter>a</parameter>
<parameter>test</parameter>
</object>).toString();

TEST(1, expect, actual);

try
{
    actual = ws.sample('this', 'is', 'a', 'test');
}
catch(e)
{
    actual = e + '';
}

expect =
(<object uri="http://localhost">
<request method="sample"/>
<parameter>this</parameter>
<parameter>is</parameter>
<parameter>a</parameter>
<parameter>test</parameter>
<parameter>this</parameter>
<parameter>is</parameter>
<parameter>a</parameter>
<parameter>test</parameter>
</object>).toString();

TEST(2, expect, actual);

END();
