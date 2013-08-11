/* ***** BEGIN LICENSE BLOCK *****
 * Distributed under the BSD license:
 *
 * Copyright (c) 2010, Ajax.org B.V.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Ajax.org B.V. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AJAX.ORG B.V. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

define('ace/theme/idle_fingers', ['require', 'exports', 'module' , 'ace/lib/dom'], function(require, exports, module) {

exports.isDark = true;
exports.cssClass = "ace-idle-fingers";
exports.cssText = ".ace-idle-fingers .ace_gutter {\
background: #3b3b3b;\
color: #fff\
}\
.ace-idle-fingers .ace_print-margin {\
width: 1px;\
background: #3b3b3b\
}\
.ace-idle-fingers .ace_scroller {\
background-color: #323232\
}\
.ace-idle-fingers .ace_text-layer {\
color: #FFFFFF\
}\
.ace-idle-fingers .ace_cursor {\
border-left: 2px solid #91FF00\
}\
.ace-idle-fingers .ace_overwrite-cursors .ace_cursor {\
border-left: 0px;\
border-bottom: 1px solid #91FF00\
}\
.ace-idle-fingers .ace_marker-layer .ace_selection {\
background: rgba(90, 100, 126, 0.88)\
}\
.ace-idle-fingers.ace_multiselect .ace_selection.ace_start {\
box-shadow: 0 0 3px 0px #323232;\
border-radius: 2px\
}\
.ace-idle-fingers .ace_marker-layer .ace_step {\
background: rgb(102, 82, 0)\
}\
.ace-idle-fingers .ace_marker-layer .ace_bracket {\
margin: -1px 0 0 -1px;\
border: 1px solid #404040\
}\
.ace-idle-fingers .ace_marker-layer .ace_active-line {\
background: #353637\
}\
.ace-idle-fingers .ace_gutter-active-line {\
background-color: #353637\
}\
.ace-idle-fingers .ace_marker-layer .ace_selected-word {\
border: 1px solid rgba(90, 100, 126, 0.88)\
}\
.ace-idle-fingers .ace_invisible {\
color: #404040\
}\
.ace-idle-fingers .ace_keyword,\
.ace-idle-fingers .ace_meta {\
color: #CC7833\
}\
.ace-idle-fingers .ace_constant,\
.ace-idle-fingers .ace_constant.ace_character,\
.ace-idle-fingers .ace_constant.ace_character.ace_escape,\
.ace-idle-fingers .ace_constant.ace_other,\
.ace-idle-fingers .ace_support.ace_constant {\
color: #6C99BB\
}\
.ace-idle-fingers .ace_invalid {\
color: #FFFFFF;\
background-color: #FF0000\
}\
.ace-idle-fingers .ace_fold {\
background-color: #CC7833;\
border-color: #FFFFFF\
}\
.ace-idle-fingers .ace_support.ace_function {\
color: #B83426\
}\
.ace-idle-fingers .ace_variable.ace_parameter {\
font-style: italic\
}\
.ace-idle-fingers .ace_string {\
color: #A5C261\
}\
.ace-idle-fingers .ace_string.ace_regexp {\
color: #CCCC33\
}\
.ace-idle-fingers .ace_comment {\
font-style: italic;\
color: #BC9458\
}\
.ace-idle-fingers .ace_meta.ace_tag {\
color: #FFE5BB\
}\
.ace-idle-fingers .ace_entity.ace_name {\
color: #FFC66D\
}\
.ace-idle-fingers .ace_markup.ace_underline {\
text-decoration: underline\
}\
.ace-idle-fingers .ace_collab.ace_user1 {\
color: #323232;\
background-color: #FFF980\
}\
.ace-idle-fingers .ace_indent-guide {\
background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAACCAYAAACZgbYnAAAAEklEQVQImWMwMjL6zzBz5sz/ABEUBGCqhK6UAAAAAElFTkSuQmCC) right repeat-y\
}";

var dom = require("../lib/dom");
dom.importCssString(exports.cssText, exports.cssClass);
});
