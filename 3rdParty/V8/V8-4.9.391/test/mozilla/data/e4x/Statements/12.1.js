/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s):
 *   Igor Bukanov
 *   Ethan Hugg
 *   Milen Nankov
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

gTestfile = '12.1.js';

START("12.1 - Default XML Namespace");


// Declare some namespaces ad a default namespace for the current block
var soap = new Namespace("http://schemas.xmlsoap.org/soap/envelope/");
var stock = new Namespace("http://mycompany.com/stocks");
default xml namespace = soap;

// Create an XML initializer in the default (i.e., soap) namespace
message =
<Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <Body>
        <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
            <stock:symbol>DIS</stock:symbol>
        </stock:GetLastTradePrice>
    </Body>
</Envelope>;

// Extract the soap encoding style using a QualifiedIdentifier
encodingStyle = message.@soap::encodingStyle;
TEST_XML(1, "http://schemas.xmlsoap.org/soap/encoding/", encodingStyle);

// Extract the body from the soap message using the default namespace
correct =
<Body
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
    <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
        <stock:symbol>DIS</stock:symbol>
    </stock:GetLastTradePrice>
</Body>;

body = message.Body;
TEST_XML(2, correct.toXMLString(), body);

// Change the stock symbol using the default namespace and qualified names
correct =
<Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <Body>
        <stock:GetLastTradePrice xmlns:stock="http://mycompany.com/stocks">
            <stock:symbol>MYCO</stock:symbol>
        </stock:GetLastTradePrice>
    </Body>
</Envelope>;

message.Body.stock::GetLastTradePrice.stock::symbol = "MYCO";

TEST(3, correct, message);

function scopeTest()
{
    var x = <a/>;
    TEST(4, soap.uri, x.namespace().uri);

    default xml namespace = "http://" + "someuri.org";
    x = <a/>;
    TEST(5, "http://someuri.org", x.namespace().uri);
}

scopeTest();

x = <a><b><c xmlns="">foo</c></b></a>;
TEST(6, soap.uri, x.namespace().uri);
TEST(7, soap.uri, x.b.namespace().uri);

ns = new Namespace("");
TEST(8, "foo", x.b.ns::c.toString());

x = <a foo="bar"/>;
TEST(9, soap.uri, x.namespace().uri);
TEST(10, "", x.@foo.namespace().uri);
TEST_XML(11, "bar", x.@foo);

default xml namespace = "";
x = <x/>;
ns = new Namespace("http://someuri");
default xml namespace = ns;
x.a = "foo";
TEST(12, "foo", x["a"].toString());
q = new QName("a");
TEST(13, "foo", x[q].toString());

default xml namespace = "";
x[q] = "bar";
TEST(14, "bar", x.ns::a.toString());

END();
