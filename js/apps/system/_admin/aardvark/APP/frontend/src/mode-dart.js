/* ***** BEGIN LICENSE BLOCK *****
 * Distributed under the BSD license:
 *
 * Copyright (c) 2012, Ajax.org B.V.
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
 *
 * Contributor(s):
 * 
 *
 *
 * ***** END LICENSE BLOCK ***** */

define('ace/mode/dart', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text', 'ace/tokenizer', 'ace/mode/dart_highlight_rules', 'ace/mode/folding/cstyle'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var Tokenizer = require("../tokenizer").Tokenizer;
var DartHighlightRules = require("./dart_highlight_rules").DartHighlightRules;
var CStyleFoldMode = require("./folding/cstyle").FoldMode;

var Mode = function() {
    var highlighter = new DartHighlightRules();
    this.foldingRules = new CStyleFoldMode();

    this.$tokenizer = new Tokenizer(highlighter.getRules());
};
oop.inherits(Mode, TextMode);

(function() {
}).call(Mode.prototype);

exports.Mode = Mode;
});


define('ace/mode/dart_highlight_rules', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text_highlight_rules'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var DartHighlightRules = function() {

    var constantLanguage = "true|false|null";
    var variableLanguage = "this|super";
    var keywordControl = "try|catch|finally|throw|break|case|continue|default|do|else|for|if|in|return|switch|while|new";
    var keywordDeclaration = "abstract|class|extends|external|factory|implements|interface|get|native|operator|set|typedef";
    var storageModifier = "static|final|const";
    var storageType = "void|bool|num|int|double|Dynamic|var|String";

    var keywordMapper = this.createKeywordMapper({
        "constant.language.dart": constantLanguage,
        "variable.language.dart": variableLanguage,
        "keyword.control.dart": keywordControl,
        "keyword.declaration.dart": keywordDeclaration,
        "storage.modifier.dart": storageModifier,
        "storage.type.primitive.dart": storageType
    }, "identifier");

    var stringfill = {
        token : "string",
        regex : ".+"
    };

    this.$rules = 
        {
    "start": [
        {
            token : "comment",
            regex : /\/\/.*$/
        },
        {
            token : "comment", // multi line comment
            regex : /\/\*/,
            next : "comment"
        },
        {
            token: ["meta.preprocessor.script.dart"],
            regex: "^(#!.*)$"
        },
        {
            token: "keyword.other.import.dart",
            regex: "#(?:\\b)(?:library|import|source|resource)(?:\\b)"
        },
        {
            token : ["keyword.other.import.dart", "text"],
            regex : "(?:\\b)(prefix)(\\s*:)"
        },
        {
            regex: "\\bas\\b",
            token: "keyword.cast.dart"
        },
        {
            regex: "\\?|:",
            token: "keyword.control.ternary.dart"
        },
        {
            regex: "(?:\\b)(is\\!?)(?:\\b)",
            token: ["keyword.operator.dart"]
        },
        {
            regex: "(<<|>>>?|~|\\^|\\||&)",
            token: ["keyword.operator.bitwise.dart"]
        },
        {
            regex: "((?:&|\\^|\\||<<|>>>?)=)",
            token: ["keyword.operator.assignment.bitwise.dart"]
        },
        {
            regex: "(===?|!==?|<=?|>=?)",
            token: ["keyword.operator.comparison.dart"]
        },
        {
            regex: "((?:[+*/%-]|\\~)=)",
            token: ["keyword.operator.assignment.arithmetic.dart"]
        },
        {
            regex: "=",
            token: "keyword.operator.assignment.dart"
        },
        {
            token : "string",
            regex : "'''",
            next : "qdoc"
        }, 
        {
            token : "string",
            regex : '"""',
            next : "qqdoc"
        }, 
        {
            token : "string",
            regex : "'",
            next : "qstring"
        }, 
        {
            token : "string",
            regex : '"',
            next : "qqstring"
        }, 
        {
            regex: "(\\-\\-|\\+\\+)",
            token: ["keyword.operator.increment-decrement.dart"]
        },
        {
            regex: "(\\-|\\+|\\*|\\/|\\~\\/|%)",
            token: ["keyword.operator.arithmetic.dart"]
        },
        {
            regex: "(!|&&|\\|\\|)",
            token: ["keyword.operator.logical.dart"]
        },
        {
            token : "constant.numeric", // hex
            regex : "0[xX][0-9a-fA-F]+\\b"
        }, 
        {
            token : "constant.numeric", // float
            regex : "[+-]?\\d+(?:(?:\\.\\d*)?(?:[eE][+-]?\\d+)?)?\\b"
        }, 
        {
            token : keywordMapper,
            regex : "[a-zA-Z_$][a-zA-Z0-9_$]*\\b"
        }
    ],
    "comment" : [
        {
            token : "comment", // closing comment
            regex : ".*?\\*\\/",
            next : "start"
        }, {
            token : "comment", // comment spanning whole line
            regex : ".+"
        }
    ],
    "qdoc" : [
        {
            token : "string",
            regex : ".*?'''",
            next : "start"
        }, stringfill],

    "qqdoc" : [
        {
            token : "string",
            regex : '.*?"""',
            next : "start"
        }, stringfill],

    "qstring" : [
        {
            token : "string",
            regex : "[^\\\\']*(?:\\\\.[^\\\\']*)*'",
            next : "start"
        }, stringfill],

    "qqstring" : [
        {
            token : "string",
            regex : '[^\\\\"]*(?:\\\\.[^\\\\"]*)*"',
            next : "start"
        }, stringfill]
}

};

oop.inherits(DartHighlightRules, TextHighlightRules);

exports.DartHighlightRules = DartHighlightRules;
});

define('ace/mode/folding/cstyle', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/range', 'ace/mode/folding/fold_mode'], function(require, exports, module) {


var oop = require("../../lib/oop");
var Range = require("../../range").Range;
var BaseFoldMode = require("./fold_mode").FoldMode;

var FoldMode = exports.FoldMode = function() {};
oop.inherits(FoldMode, BaseFoldMode);

(function() {

    this.foldingStartMarker = /(\{|\[)[^\}\]]*$|^\s*(\/\*)/;
    this.foldingStopMarker = /^[^\[\{]*(\}|\])|^[\s\*]*(\*\/)/;

    this.getFoldWidgetRange = function(session, foldStyle, row) {
        var line = session.getLine(row);
        var match = line.match(this.foldingStartMarker);
        if (match) {
            var i = match.index;

            if (match[1])
                return this.openingBracketBlock(session, match[1], row, i);

            return session.getCommentFoldRange(row, i + match[0].length, 1);
        }

        if (foldStyle !== "markbeginend")
            return;

        var match = line.match(this.foldingStopMarker);
        if (match) {
            var i = match.index + match[0].length;

            if (match[1])
                return this.closingBracketBlock(session, match[1], row, i);

            return session.getCommentFoldRange(row, i, -1);
        }
    };

}).call(FoldMode.prototype);

});
