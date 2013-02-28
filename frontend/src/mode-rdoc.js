/*
 * rdoc.js
 *
 * Copyright (C) 2009-11 by RStudio, Inc.
 *
 * This program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
define('ace/mode/rdoc', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text', 'ace/tokenizer', 'ace/mode/text_highlight_rules', 'ace/mode/rdoc_highlight_rules', 'ace/mode/matching_brace_outdent'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var Tokenizer = require("../tokenizer").Tokenizer;
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;
var RDocHighlightRules = require("./rdoc_highlight_rules").RDocHighlightRules;
var MatchingBraceOutdent = require("./matching_brace_outdent").MatchingBraceOutdent;

var Mode = function(suppressHighlighting) {
	this.$tokenizer = new Tokenizer(new RDocHighlightRules().getRules());
    this.$outdent = new MatchingBraceOutdent();
};
oop.inherits(Mode, TextMode);

(function() {
    this.getNextLineIndent = function(state, line, tab) {
        return this.$getIndent(line);
    };
}).call(Mode.prototype);

exports.Mode = Mode;
});
define('ace/mode/rdoc_highlight_rules', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/lib/lang', 'ace/mode/text_highlight_rules', 'ace/mode/latex_highlight_rules'], function(require, exports, module) {


var oop = require("../lib/oop");
var lang = require("../lib/lang");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;
var LaTeXHighlightRules = require("./latex_highlight_rules");

var RDocHighlightRules = function() {

    this.$rules = {
        "start" : [
	        {
	            token : "comment",
	            regex : "%.*$"
	        }, {
	            token : "text", // non-command
	            regex : "\\\\[$&%#\\{\\}]"
	        }, {
	            token : "keyword", // command
	            regex : "\\\\(?:name|alias|method|S3method|S4method|item|code|preformatted|kbd|pkg|var|env|option|command|author|email|url|source|cite|acronym|href|code|preformatted|link|eqn|deqn|keyword|usage|examples|dontrun|dontshow|figure|if|ifelse|Sexpr|RdOpts|inputencoding|usepackage)\\b",
               next : "nospell"
	        }, {
	            token : "keyword", // command
	            regex : "\\\\(?:[a-zA-z0-9]+|[^a-zA-z0-9])"
	        }, {
               token : "paren.keyword.operator",
	            regex : "[[({]"
	        }, {
               token : "paren.keyword.operator",
	            regex : "[\\])}]"
	        }, {
	            token : "text",
	            regex : "\\s+"
	        }
        ],
        "nospell" : [
           {
               token : "comment",
               regex : "%.*$",
               next : "start"
           }, {
               token : "nospell.text", // non-command
               regex : "\\\\[$&%#\\{\\}]"
           }, {
               token : "keyword", // command
               regex : "\\\\(?:name|alias|method|S3method|S4method|item|code|preformatted|kbd|pkg|var|env|option|command|author|email|url|source|cite|acronym|href|code|preformatted|link|eqn|deqn|keyword|usage|examples|dontrun|dontshow|figure|if|ifelse|Sexpr|RdOpts|inputencoding|usepackage)\\b"
           }, {
               token : "keyword", // command
               regex : "\\\\(?:[a-zA-z0-9]+|[^a-zA-z0-9])",
               next : "start"
           }, {
               token : "paren.keyword.operator",
               regex : "[[({]"
           }, {
               token : "paren.keyword.operator",
               regex : "[\\])]"
           }, {
               token : "paren.keyword.operator",
               regex : "}",
               next : "start"
           }, {
               token : "nospell.text",
               regex : "\\s+"
           }, {
               token : "nospell.text",
               regex : "\\w+"
           }
        ]
    };
};

oop.inherits(RDocHighlightRules, TextHighlightRules);

exports.RDocHighlightRules = RDocHighlightRules;
});
define('ace/mode/latex_highlight_rules', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text_highlight_rules'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var LatexHighlightRules = function() {   
    this.$rules = {
        "start" : [{
            token : "keyword",
            regex : "\\\\(?:[^a-zA-Z]|[a-zA-Z]+)"
        }, {
            token : "lparen",
            regex : "[[({]"
        }, {
            token : "rparen",
            regex : "[\\])}]"
        }, {
            token : "string",
            regex : "\\$(?:(?:\\\\.)|(?:[^\\$\\\\]))*?\\$"
        }, {
            token : "comment",
            regex : "%.*$"
        }]
    };
};

oop.inherits(LatexHighlightRules, TextHighlightRules);

exports.LatexHighlightRules = LatexHighlightRules;

});

define('ace/mode/matching_brace_outdent', ['require', 'exports', 'module' , 'ace/range'], function(require, exports, module) {


var Range = require("../range").Range;

var MatchingBraceOutdent = function() {};

(function() {

    this.checkOutdent = function(line, input) {
        if (! /^\s+$/.test(line))
            return false;

        return /^\s*\}/.test(input);
    };

    this.autoOutdent = function(doc, row) {
        var line = doc.getLine(row);
        var match = line.match(/^(\s*\})/);

        if (!match) return 0;

        var column = match[1].length;
        var openBracePos = doc.findMatchingBracket({row: row, column: column});

        if (!openBracePos || openBracePos.row == row) return 0;

        var indent = this.$getIndent(doc.getLine(openBracePos.row));
        doc.replace(new Range(row, 0, row, column-1), indent);
    };

    this.$getIndent = function(line) {
        var match = line.match(/^(\s+)/);
        if (match) {
            return match[1];
        }

        return "";
    };

}).call(MatchingBraceOutdent.prototype);

exports.MatchingBraceOutdent = MatchingBraceOutdent;
});
