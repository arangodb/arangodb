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

define('ace/ext/searchbox', ['require', 'exports', 'module' , 'ace/lib/dom', 'ace/lib/lang', 'ace/lib/event', 'ace/keyboard/hash_handler', 'ace/lib/keys'], function(require, exports, module) {


var dom = require("../lib/dom");
var lang = require("../lib/lang");
var event = require("../lib/event");
var searchboxCss = "\
/* ------------------------------------------------------------------------------------------\
* Editor Search Form\
* --------------------------------------------------------------------------------------- */\
.ace_search {\
background-color: #ddd;\
border: 1px solid #cbcbcb;\
border-top: 0 none;\
max-width: 297px;\
overflow: hidden;\
margin: 0;\
padding: 4px;\
padding-right: 6px;\
padding-bottom: 0;\
position: absolute;\
top: 0px;\
z-index: 99;\
}\
.ace_search.left {\
border-left: 0 none;\
border-radius: 0px 0px 5px 0px;\
left: 0;\
}\
.ace_search.right {\
border-radius: 0px 0px 0px 5px;\
border-right: 0 none;\
right: 0;\
}\
.ace_search_form, .ace_replace_form {\
border-radius: 3px;\
border: 1px solid #cbcbcb;\
float: left;\
margin-bottom: 4px;\
overflow: hidden;\
}\
.ace_search_field {\
background-color: white;\
border-right: 1px solid #cbcbcb;\
border: 0 none;\
-webkit-box-sizing: border-box;\
-moz-box-sizing: border-box;\
box-sizing: border-box;\
display: block;\
float: left;\
height: 22px;\
outline: 0;\
padding: 0 7px;\
width: 214px;\
margin: 0;\
}\
.ace_searchbtn,\
.ace_replacebtn {\
background: #fff;\
border: 0 none;\
border-left: 1px solid #dcdcdc;\
cursor: pointer;\
display: block;\
float: left;\
height: 22px;\
margin: 0;\
padding: 0;\
position: relative;\
}\
.ace_searchbtn:last-child,\
.ace_replacebtn:last-child {\
border-top-right-radius: 3px;\
border-bottom-right-radius: 3px;\
}\
.ace_searchbtn:disabled {\
background: none;\
cursor: default;\
}\
.ace_searchbtn {\
background-position: 50% 50%;\
background-repeat: no-repeat;\
width: 27px;\
}\
.ace_searchbtn.prev {\
background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAFCAYAAAB4ka1VAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAADFJREFUeNpiSU1NZUAC/6E0I0yACYskCpsJiySKIiY0SUZk40FyTEgCjGgKwTRAgAEAQJUIPCE+qfkAAAAASUVORK5CYII=);    \
}\
.ace_searchbtn.next {\
background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAFCAYAAAB4ka1VAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAADRJREFUeNpiTE1NZQCC/0DMyIAKwGJMUAYDEo3M/s+EpvM/mkKwCQxYjIeLMaELoLMBAgwAU7UJObTKsvAAAAAASUVORK5CYII=);    \
}\
.ace_searchbtn_close {\
background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA4AAAAcCAYAAABRVo5BAAAAZ0lEQVR42u2SUQrAMAhDvazn8OjZBilCkYVVxiis8H4CT0VrAJb4WHT3C5xU2a2IQZXJjiQIRMdkEoJ5Q2yMqpfDIo+XY4k6h+YXOyKqTIj5REaxloNAd0xiKmAtsTHqW8sR2W5f7gCu5nWFUpVjZwAAAABJRU5ErkJggg==) no-repeat 50% 0;\
border-radius: 50%;\
border: 0 none;\
color: #656565;\
cursor: pointer;\
display: block;\
float: right;\
font-family: Arial;\
font-size: 16px;\
height: 14px;\
line-height: 16px;\
margin: 5px 1px 9px 5px;\
padding: 0;\
text-align: center;\
width: 14px;\
}\
.ace_searchbtn_close:hover {\
background-color: #656565;\
background-position: 50% 100%;\
color: white;\
}\
.ace_replacebtn.prev {\
width: 54px\
}\
.ace_replacebtn.next {\
width: 27px\
}";
var HashHandler = require("../keyboard/hash_handler").HashHandler;
var keyUtil = require("../lib/keys");

dom.importCssString(searchboxCss, "ace_searchbox");

var html = '<div class="ace_search right">\
    <button action="hide" class="ace_searchbtn_close"></button>\
    <div class="ace_search_form">\
        <input class="ace_search_field" placeholder="Search for" spellcheck="false">\
        <button action="findNext" class="ace_searchbtn next"></button>\
        <button action="findPrev" class="ace_searchbtn prev"></button>\
    </div>\
    <div class="ace_replace_form">\
        <input class="ace_search_field" placeholder="Replace with" spellcheck="false">\
        <button action="replace" class="ace_replacebtn">Replace</button>\
        <button action="replaceAll" class="ace_replacebtn">All</button>\
    </div>\
</div>'.replace(/>\s+/g, ">");

var SearchBox = function(editor, range, showReplaceForm) {
    var div = dom.createElement("div");
    div.innerHTML = html;
    this.element = div.firstChild;

    this.$init();
    this.setEditor(editor);
};

(function() {
    this.setEditor = function(editor) {
        editor.searchBox = this;
        editor.container.appendChild(this.element);
        this.editor = editor;
    };

    this.$init = function() {
        var sb = this.element;

        this.searchBox = sb.querySelector(".ace_search_form");
        this.replaceBox = sb.querySelector(".ace_replace_form");
        this.searchInput = this.searchBox.querySelector(".ace_search_field");
        this.replaceInput = this.replaceBox.querySelector(".ace_search_field");

        var _this = this;
        event.addListener(sb, "mousedown", function(e) {
            setTimeout(function(){
                _this.activeInput.focus();
            }, 0);
            event.stopPropagation(e);
        });
        event.addListener(sb, "click", function(e) {
            var t = e.target;
            var action = t.getAttribute("action");
            if (action && _this[action])
                _this[action]();
            event.stopPropagation(e);
        });

        event.addCommandKeyListener(sb, function(e, hashId, keyCode) {
            var keyString = keyUtil.keyCodeToString(keyCode);
            var command = _this.$searchBarKb.findKeyCommand(hashId, keyString);
            if (command && command.exec) {
                command.exec(_this);
                event.stopEvent(e);
            }
        });

        this.$onChange = lang.delayedCall(function() {
            _this.find(false, false);
        });

        event.addListener(this.searchInput, "input", function() {
            _this.$onChange.schedule(20);
        });
        event.addListener(this.searchInput, "focus", function() {
            _this.activeInput = _this.searchInput;
        });
        event.addListener(this.replaceInput, "focus", function() {
            _this.activeInput = _this.replaceInput;
        });
    };
    this.$closeSearchBarKb = new HashHandler([{
        bindKey: "Esc",
        name: "closeSearchBar",
        exec: function(editor) {
            editor.searchBox.hide();
        }
    }]);
    this.$searchBarKb = new HashHandler();
    this.$searchBarKb.bindKeys({
        "Ctrl-f|Command-f|Ctrl-H|Command-Option-F": function(sb) {
            var isReplace = sb.isReplace = !sb.isReplace;
            sb.replaceBox.style.display = isReplace ? "" : "none";
            sb[isReplace ? "replaceInput" : "searchInput"].focus();
        },
        "esc": function(sb) {
            setTimeout(function() { sb.hide();});
        },
        "Return": function(sb) {
            if (sb.activeInput == sb.replaceInput)
                sb.replace();
            sb.findNext();
        },
        "Shift-Return": function(sb) {
            if (sb.activeInput == sb.replaceInput)
                sb.replace();
            sb.findPrev();
        },
        "Tab": function(sb) {
            (sb.activeInput == sb.replaceInput ? sb.searchInput : sb.replaceInput).focus();
        }
    });


    this.find = function(skipCurrent, backwards) {
        this.editor.find(this.searchInput.value, {
            skipCurrent: skipCurrent,
            backwards: backwards,
            wrap: true
        });
        this.editor.session.highlight(this.editor.$search.$options.re);
    };
    this.findNext = function() {
        this.find(true, false);
    };
    this.findPrev = function() {
        this.find(true, true);
    };
    this.replace = function() {
        this.editor.replace(this.replaceInput.value);
        this.findNext();
    };
    this.replaceAll = function() {
        this.editor.replaceAll(this.replaceInput.value);
    };

    this.hide = function () {
        this.element.style.display = "none";
        this.editor.keyBinding.removeKeyboardHandler(this.$searchKeybingin);
        this.editor.focus();
    };
    this.show = function(value, isReplace) {
        this.element.style.display = "";
        this.replaceBox.style.display = isReplace ? "" : "none";

        this.isReplace = isReplace;

        if (value)
            this.searchInput.value = value;
        this.searchInput.focus();
        this.searchInput.select();

        this.editor.keyBinding.addKeyboardHandler(this.$closeSearchBarKb);
    };

}).call(SearchBox.prototype);

exports.SearchBox = SearchBox;

exports.Search = function(editor, isReplace) {
    var sb = editor.searchBox || new SearchBox(editor);
    sb.show(editor.session.getTextRange(), isReplace);
};


exports.ISearch = function(session, options) {
    this.$changeListener = this.$changeListener.bind(this);
    this.startRange = session.selection.toOrientedRange();
    this.options = options || {};
};

(function(){
    this.setSession = function(session) {
        if (this.session) {
            this.session.removeListener(this.$changeListener);
        }
        this.session = session;
        this.session.addListener(this.$changeListener);
    };
    this.setSearchString = function() {

    };
    this.getValue = function() {
        if (this.value == null)
            this.value = this.session.getValue();
        return this.value;
    };
    this.$changeListener = function() {
        this.value = null;
    };
    this.find = function() {

    };
    this.$edgeBefore = function() {
        this.cursor = this.startRange[this.options.backwards ? "start" : "end"];
    };
    this.$edgeAfter = function() {

    };
    this.next = function(dir) {

    };
}).call(exports.ISearch.prototype);


});