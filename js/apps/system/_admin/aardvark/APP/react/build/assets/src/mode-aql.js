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

define('ace/mode/aql', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text', 'ace/tokenizer', 'ace/mode/aql_highlight_rules', 'ace/range'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var Tokenizer = require("../tokenizer").Tokenizer;
var AqlHighlightRules = require("./aql_highlight_rules").AqlHighlightRules;
var Range = require("../range").Range;

var Mode = function() {
    this.$tokenizer = new Tokenizer(new AqlHighlightRules().getRules());
};
oop.inherits(Mode, TextMode);

(function() {

    this.toggleCommentLines = function(state, doc, startRow, endRow) {
        var outdent = true;
        var outentedRows = [];
        var re = /^(\s*)\/\//;

        for (var i=startRow; i<= endRow; i++) {
            if (!re.test(doc.getLine(i))) {
                outdent = false;
                break;
            }
        }

        if (outdent) {
            var deleteRange = new Range(0, 0, 0, 0);
            for (var i=startRow; i<= endRow; i++)
            {
                var line = doc.getLine(i);
                var m = line.match(re);
                deleteRange.start.row = i;
                deleteRange.end.row = i;
                deleteRange.end.column = m[0].length;
                doc.replace(deleteRange, m[1]);
            }
        }
        else {
            doc.indentRows(startRow, endRow, "//");
        }
    };

}).call(Mode.prototype);

exports.Mode = Mode;

});

define('ace/mode/aql_highlight_rules', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/mode/text_highlight_rules'], function(require, exports, module) {


var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var AqlHighlightRules = function() {

    var keywords = (
        "for|return|filter|search|sort|limit|let|collect|asc|desc|in|into|insert|update|remove|replace|upsert|options|with|and|or|not|distinct|graph|shortest_path|outbound|inbound|any|all|none|aggregate|like|k_shortest_paths|prune"
    );

    var builtinFunctions = (
        "(to_bool|to_number|to_string|to_list|is_null|is_bool|is_number|is_string|is_list|is_document|typename|json_stringify|json_parse|" +
        "concat|concat_separator|char_length|lower|upper|substring|left|right|trim|reverse|contains|" +
        "log|log2|log10|exp|exp2|sin|cos|tan|asin|acos|atan|atan2|radians|degrees|pi|regex_test|" +
        "like|floor|ceil|round|abs|rand|sqrt|pow|length|count|min|max|average|sum|product|median|variance_population|" +
        "variance_sample|first|last|unique|outersection|interleave|in_range|jaccard|matches|merge|merge_recursive|has|attributes|values|unset|unset_recursive|keep|" +
        "near|within|within_rectangle|is_in_polygon|distance|fulltext|paths|traversal|traversal_tree|edges|stddev_sample|stddev_population|" +
        "slice|nth|position|translate|zip|call|apply|push|append|pop|shift|unshift|remove_value|remove_values|" + 
        "remove_nth|replace_nth|date_now|date_timestamp|date_iso8601|date_dayofweek|date_year|date_month|date_day|date_hour|" +
        "date_minute|date_second|date_millisecond|date_dayofyear|date_isoweek|date_leapyear|date_quarter|date_days_in_month|" + 
        "date_add|date_subtract|date_diff|date_compare|date_format|fail|passthru|sleep|not_null|" +
        "first_list|first_document|parse_identifier|current_user|current_database|" +
        "collections|document|decode_rev|union|union_distinct|intersection|flatten|is_same_collection|check_document|" +
        "ltrim|rtrim|find_first|find_last|split|substitute|ipv4_to_number|ipv4_from_number|is_ipv4|md5|sha1|crc32|fnv64|hash|random_token|to_base64|" +
        "to_hex|encode_uri_component|soundex|assert|warn|is_key|sorted|sorted_unique|count_distinct|" +
        "levenshtein_distance|levenshtein_match|regex_matches|regex_split|ngram_match|ngram_similarity|ngram_positional_similarity|uuid|" +
        "tokens|exists|starts_with|phrase|min_match|boost|analyzer|" +
        "to_hex|encode_uri_component|soundex|assert|warn|is_key|sorted|sorted_unique|count_distinct|" +
        "geo_point|geo_multipoint|geo_polygon|geo_linestring|geo_multilinestring|geo_contains|geo_intersects|" +
        "geo_equals|geo_distance|geo_area|AQL_LAST_ENTRY)"
    );

    var keywordMapper = this.createKeywordMapper({
        "support.function": builtinFunctions,
        "keyword": keywords,
        "constant.language": "null",
        "constant.language.boolean": "true|false"
    }, "identifier", true);

    this.$rules = {
        "start" : [ {
           token : "comment",
           regex : /\/\/.*$/
        }, {
            token : "comment",
            regex : /\/\*/,
            next : "comment_ml"
        }, {
            token : "string",           // " string
            regex : '".*?"'
        }, {
            token : "string",           // ' string
            regex : "'.*?'"
        }, {
            token : "string",           // ` string
            regex : "`.*?`"
        }, {
            token : "constant.numeric", // float
            regex : "[+-]?\\d+(?:(?:\\.\\d*)?(?:[eE][+-]?\\d+)?)?\\b"
        }, {
            token : keywordMapper,
            regex : "[a-zA-Z_][a-zA-Z0-9_]*\\b"
        }, {
            token : "keyword.operator",
            regex : "\\+|\\-|\\/|\\/\\/|%|@>|<@|&&|\\|\\||!|<|>|<=|=>|==|!=|=|\\[\\*\\]"
        }, {
            token : "paren.lparen",
            regex : "[\\(\\{]"
        }, {
            token : "paren.rparen",
            regex : "[\\)\\}]"
        }, {
            token : "text",
            regex : "\\s+"
        } ],
        "comment_ml" : [ {
            token : "comment", 
            regex : /\*\//,
            next : "start",
        }, {
          defaultToken : "comment"
        } ]
    };
};

oop.inherits(AqlHighlightRules, TextHighlightRules);

exports.AqlHighlightRules = AqlHighlightRules;
});

