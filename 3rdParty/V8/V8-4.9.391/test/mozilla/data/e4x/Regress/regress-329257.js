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
 * David P. Caldwell.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Brown
 *   David P. Caldwell <inonit@inonit.com>
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

gTestfile = 'regress-329257.js';

var BUGNUMBER = 329257;
var summary = "namespace prefix in E4X dot query";

printBugNumber(BUGNUMBER);
START(summary);

var ns12 = new Namespace("foo");
var nestInfo = <a xmlns="foo">
	<ParameterAvailabilityInfo>
		<ParameterID>10</ParameterID>
		<PowerOfTen>1</PowerOfTen>
	</ParameterAvailabilityInfo>
	<ParameterAvailabilityInfo>
		<ParameterID>100</ParameterID>
		<PowerOfTen>2</PowerOfTen>
	</ParameterAvailabilityInfo>
	<ParameterAvailabilityInfo>
		<ParameterID>1000</ParameterID>
		<PowerOfTen>3</PowerOfTen>
	</ParameterAvailabilityInfo>
</a>;

var paramInfo = nestInfo.ns12::ParameterAvailabilityInfo;
var paramInfo100 = nestInfo.ns12::ParameterAvailabilityInfo.(ns12::ParameterID == 100);
TEST(1, 2, Number(paramInfo100.ns12::PowerOfTen));

default xml namespace = ns12;
var paramInfo100 = nestInfo.ParameterAvailabilityInfo.(ParameterID == 100);
TEST(2, 2, Number(paramInfo100.ns12::PowerOfTen));
