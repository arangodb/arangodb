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

gTestfile = '11.6.1.js';

START("11.6.1 - XML Assignment");

// Change the value of the id attribute on the second item
order =
<order>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

correct =
<order>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="1.23">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

order.item[1].@id = 1.23;
TEST(1, correct, order);

// Add a new attribute to the second item
order =
<order>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

correct =
<order>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2" newattr="new value">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

order.item[1].@newattr = "new value";
TEST(2, correct, order);

// Construct an attribute list containing all the ids in this order
order =
<order>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

order.@allids = order.item.@id;   
TEST_XML(3, "1 2 3 4", order.@allids);

// Replace first child of the order element with an XML value
order =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;


order.*[0] =
<customer>
    <name>Fred</name>
    <address>123 Foobar Ave.</address>
    <city>Bellevue</city>
    <state>WA</state>
</customer>;

correct =
<order>
    <customer>
       <name>Fred</name>
       <address>123 Foobar Ave.</address>
       <city>Bellevue</city>
       <state>WA</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

TEST(4, correct, order);

// Replace the second child of the order element with a list of items

order =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

correct =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item>item one</item>
    <item>item two</item>
    <item>item three</item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

order.item[0] = <item>item one</item> +
           <item>item two</item> +
           <item>item three</item>;

TEST(5, correct, order);

// Replace the third child of the order with a text node
order =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

correct =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">A Text Node</item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

order.item[1] = "A Text Node";

TEST(6, correct, order);

// append a new item to the end of the order

order =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
</order>;

correct =
<order>
    <customer>
        <name>John</name>
        <address>948 Ranier Ave.</address>
        <city>Portland</city>
        <state>OR</state>
    </customer>
    <item id="1">
        <description>Big Screen Television</description>
        <price>1299.99</price>
    </item>
    <item id="2">
        <description>DVD Player</description>
        <price>399.99</price>
    </item>
    <item id="3">
        <description>CD Player</description>
        <price>199.99</price>
    </item>
    <item id="4">
        <description>8-Track Player</description>
        <price>69.99</price>
    </item>
    <item>new item</item>
</order>;

order.*[order.*.length()] = <item>new item</item>;

TEST(7, correct, order);

// Change the price of the item
item =
<item>
    <description>Big Screen Television</description>
    <price>1299.99</price>
</item>

correct =
<item>
    <description>Big Screen Television</description>
    <price>99.95</price>
</item>

item.price = 99.95;

TEST(8, item, correct);

// Change the description of the item
item =
<item>
    <description>Big Screen Television</description>
    <price>1299.99</price>
</item>

correct =
<item>
    <description>Mobile Phone</description>
    <price>1299.99</price>
</item>

item.description = "Mobile Phone";

TEST(9, item, correct);

END();
