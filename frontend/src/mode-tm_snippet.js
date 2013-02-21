define('ace/mode/tm_snippet', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text', 'ace/tokenizer', 'ace/mode/text_highlight_rules'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var Tokenizer = require("../tokenizer").Tokenizer;
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var SnippetHighlightRules = function() {

    var builtins = "SELECTION|CURRENT_WORD|SELECTED_TEXT|CURRENT_LINE|LINE_INDEX|" +
        "LINE_NUMBER|SOFT_TABS|TAB_SIZE|FILENAME|FILEPATH|FULLNAME";

    this.$rules = {
        "start" : [
            {token:"constant.language.escape", regex: /\\[\$}`\\]/},
            {token:"keyword", regex: "\\$(?:TM_)?(?:" + builtins + ")\\b"},
            {token:"variable", regex: "\\$\\w+"},
            {token: function(value, state, stack) {
                if (stack[1])
                    stack[1]++;
                else
                    stack.unshift("start", 1);
                return this.tokenName;
            }, tokenName: "markup.list", regex: "\\${", next: "varDecl"},
            {token: function(value, state, stack) {
                if (!stack[1])
                    return "text";
                stack[1]--;
                if (!stack[1])
                    stack.splice(0,2);
                return this.tokenName;
            }, tokenName: "markup.list", regex: "}"},
            {token: "doc,comment", regex:/^\${2}-{5,}$/}
        ],
        "varDecl" : [
            {regex: /\d+\b/, token: "constant.numeric"},
            {token:"keyword", regex: "(?:TM_)?(?:" + builtins + ")\\b"},
            {token:"variable", regex: "\\w+"},
            {regex: /:/, token: "punctuation.operator", next: "start"},
            {regex: /\//, token: "string.regex", next: "regexp"},
            {regex: "", next: "start"}
        ],
        "regexp" : [
            {regex: /\\./, token: "escape"},
            {regex: /\[/, token: "regex.start", next: "charClass"},
            {regex: "/", token: "string.regex", next: "format"},
            {"token": "string.regex", regex:"."},
        ],
        charClass : [
            {regex: "\\.", token: "escape"},
            {regex: "\\]", token: "regex.end", next: "regexp"},
            {"token": "string.regex", regex:"."},
        ],
        "format" : [
            {regex: /\\[ulULE]/, token: "keyword"},
            {regex: /\$\d+/, token: "variable"},
            {regex: "/[gim]*:?", token: "string.regex", next: "start"},
            {"token": "string", regex:"."},
        ]
    };
};

oop.inherits(SnippetHighlightRules, TextHighlightRules);

exports.SnippetHighlightRules = SnippetHighlightRules;


var Mode = function() {
    var highlighter = new SnippetHighlightRules();

    this.$tokenizer = new Tokenizer(highlighter.getRules());
};
oop.inherits(Mode, TextMode);

(function() {
    this.getNextLineIndent = function(state, line, tab) {
        return this.$getIndent(line);
    };
}).call(Mode.prototype);
exports.Mode = Mode;


});
