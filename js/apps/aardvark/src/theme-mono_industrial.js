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

define('ace/theme/mono_industrial', ['require', 'exports', 'module' , 'ace/lib/dom'], function(require, exports, module) {

exports.isDark = true;
exports.cssClass = "ace-mono-industrial";
exports.cssText = ".ace-mono-industrial .ace_gutter {\
background: #1d2521;\
color: #C5C9C9\
}\
.ace-mono-industrial .ace_print-margin {\
width: 1px;\
background: #555651\
}\
.ace-mono-industrial .ace_scroller {\
background-color: #222C28\
}\
.ace-mono-industrial .ace_text-layer {\
color: #FFFFFF\
}\
.ace-mono-industrial .ace_cursor {\
border-left: 2px solid #FFFFFF\
}\
.ace-mono-industrial .ace_overwrite-cursors .ace_cursor {\
border-left: 0px;\
border-bottom: 1px solid #FFFFFF\
}\
.ace-mono-industrial .ace_marker-layer .ace_selection {\
background: rgba(145, 153, 148, 0.40)\
}\
.ace-mono-industrial.ace_multiselect .ace_selection.ace_start {\
box-shadow: 0 0 3px 0px #222C28;\
border-radius: 2px\
}\
.ace-mono-industrial .ace_marker-layer .ace_step {\
background: rgb(102, 82, 0)\
}\
.ace-mono-industrial .ace_marker-layer .ace_bracket {\
margin: -1px 0 0 -1px;\
border: 1px solid rgba(102, 108, 104, 0.50)\
}\
.ace-mono-industrial .ace_marker-layer .ace_active-line {\
background: rgba(12, 13, 12, 0.25)\
}\
.ace-mono-industrial .ace_gutter-active-line {\
background-color: rgba(12, 13, 12, 0.25)\
}\
.ace-mono-industrial .ace_marker-layer .ace_selected-word {\
border: 1px solid rgba(145, 153, 148, 0.40)\
}\
.ace-mono-industrial .ace_invisible {\
color: rgba(102, 108, 104, 0.50)\
}\
.ace-mono-industrial .ace_string {\
background-color: #151C19;\
color: #FFFFFF\
}\
.ace-mono-industrial .ace_keyword,\
.ace-mono-industrial .ace_meta {\
color: #A39E64\
}\
.ace-mono-industrial .ace_constant,\
.ace-mono-industrial .ace_constant.ace_character,\
.ace-mono-industrial .ace_constant.ace_character.ace_escape,\
.ace-mono-industrial .ace_constant.ace_numeric,\
.ace-mono-industrial .ace_constant.ace_other {\
color: #E98800\
}\
.ace-mono-industrial .ace_entity.ace_name.ace_function,\
.ace-mono-industrial .ace_keyword.ace_operator,\
.ace-mono-industrial .ace_variable {\
color: #A8B3AB\
}\
.ace-mono-industrial .ace_invalid {\
color: #FFFFFF;\
background-color: rgba(153, 0, 0, 0.68)\
}\
.ace-mono-industrial .ace_support.ace_constant {\
color: #C87500\
}\
.ace-mono-industrial .ace_fold {\
background-color: #A8B3AB;\
border-color: #FFFFFF\
}\
.ace-mono-industrial .ace_support.ace_function {\
color: #588E60\
}\
.ace-mono-industrial .ace_entity.ace_name,\
.ace-mono-industrial .ace_support.ace_class,\
.ace-mono-industrial .ace_support.ace_type {\
color: #5778B6\
}\
.ace-mono-industrial .ace_storage {\
color: #C23B00\
}\
.ace-mono-industrial .ace_variable.ace_language,\
.ace-mono-industrial .ace_variable.ace_parameter {\
color: #648BD2\
}\
.ace-mono-industrial .ace_comment {\
color: #666C68;\
background-color: #151C19\
}\
.ace-mono-industrial .ace_entity.ace_other.ace_attribute-name {\
color: #909993\
}\
.ace-mono-industrial .ace_markup.ace_underline {\
text-decoration: underline\
}\
.ace-mono-industrial .ace_entity.ace_name.ace_tag {\
color: #A65EFF\
}\
.ace-mono-industrial .ace_indent-guide {\
background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAACCAYAAACZgbYnAAAAEklEQVQImWNQ0tH4zzBz5sz/ABAOBECKH+evAAAAAElFTkSuQmCC) right repeat-y\
}\
";

var dom = require("../lib/dom");
dom.importCssString(exports.cssText, exports.cssClass);
});
