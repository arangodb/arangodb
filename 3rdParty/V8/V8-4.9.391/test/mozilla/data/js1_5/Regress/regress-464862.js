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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-464862.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 464862;
var summary = 'Do not assert: ( int32_t(delta) == uint8_t(delta) )';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function ygTreeView(id) {
  this.init(id);
}

ygTreeView.prototype.init = function (id) {this.root = new ygRootNode(this);};

function ygNode() {}

ygNode.prototype.nextSibling = null;

ygNode.prototype.init = function (_32, _33, _34) {
  this.children = [];
  this.expanded = _34;
  if (_33) {
    this.tree = _33.tree;
    this.depth = _33.depth + 1;
    _33.appendChild(this);
  }
};

ygNode.prototype.appendChild = function (_35) {
  if (this.hasChildren()) {
    var sib = this.children[this.children.length - 1];
  }
  this.children[this.children.length] = _35;
};

ygNode.prototype.getElId = function () {};

ygNode.prototype.getNodeHtml = function () {};

ygNode.prototype.getToggleElId = function () {};

ygNode.prototype.getStyle = function () {
  var loc = this.nextSibling ? "t" : "l";
  var _39 = "n";
  if (this.hasChildren(true)) {}
};

ygNode.prototype.hasChildren = function () {return this.children.length > 0;};

ygNode.prototype.getHtml = function () {
  var sb = [];
  sb[sb.length] = "<div class=\"ygtvitem\" id=\"" + this.getElId() + "\">";
  sb[sb.length] = this.getNodeHtml();
  sb[sb.length] = this.getChildrenHtml();
};

ygNode.prototype.getChildrenHtml = function () {
  var sb = [];
  if (this.hasChildren(true) && this.expanded) {
    sb[sb.length] = this.renderChildren();
  }
};

ygNode.prototype.renderChildren = function () {return this.completeRender();};

ygNode.prototype.completeRender = function () {
  var sb = [];
  for (var i = 0; i < this.children.length; ++i) {
    sb[sb.length] = this.children[i].getHtml();
  }
};

ygRootNode.prototype = new ygNode;

function ygRootNode(_48) {
  this.init(null, null, true);
}

ygTextNode.prototype = new ygNode;

function ygTextNode(_49, _50, _51) {
  this.init(_49, _50, _51);
  this.setUpLabel(_49);
}

ygTextNode.prototype.setUpLabel = function (_52) {
  if (typeof _52 == "string") {}
  if (_52.target) {}
  this.labelElId = "ygtvlabelel" + this.index;
};

ygTextNode.prototype.getNodeHtml = function () {
  var sb = new Array;
  sb[sb.length] = "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\">";
  sb[sb.length] = "<tr>";
  for (i = 0; i < this.depth; ++i) {}
  sb[sb.length] = " id=\"" + this.getToggleElId() + "\"";
  sb[sb.length] = " class=\"" + this.getStyle() + "\"";
  if (this.hasChildren(true)) {}
  sb[sb.length] = " id=\"" + this.labelElId + "\"";
};

function buildUserTree() {
  userTree = new ygTreeView("userTree");
  addMenuNode(userTree, "N", "navheader");
  addMenuNode(userTree, "R", "navheader");
  addMenuNode(userTree, "S", "navheader");
}

function addMenuNode(tree, label, styleClass) {
  new ygTextNode({}, tree.root, false);
}

buildUserTree();
userTree.root.getHtml();

reportCompare(expect, actual, summary);
