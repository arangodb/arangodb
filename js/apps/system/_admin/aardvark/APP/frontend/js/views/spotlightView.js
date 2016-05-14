/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.SpotlightView = Backbone.View.extend({

    template: templateEngine.createTemplate("spotlightView.ejs"),

    el: "#spotlightPlaceholder",

    displayLimit: 8,
    typeahead: null,
    callbackSuccess: null,
    callbackCancel: null,

    collections: {
      system: [],
      doc: [],
      edge: [],
    },

    events: {
      "focusout #spotlight .tt-input" : "hide",
      "keyup #spotlight .typeahead" : "listenKey"
    },

    aqlKeywordsArray: [],
    aqlBuiltinFunctionsArray: [],

    aqlKeywords: 
      "for|return|filter|sort|limit|let|collect|asc|desc|in|into|" + 
      "insert|update|remove|replace|upsert|options|with|and|or|not|" + 
      "distinct|graph|outbound|inbound|any|all|none|aggregate|like|count",

    aqlBuiltinFunctions: 
"to_bool|to_number|to_string|to_list|is_null|is_bool|is_number|is_string|is_list|is_document|typename|" +
"concat|concat_separator|char_length|lower|upper|substring|left|right|trim|reverse|contains|" +
"like|floor|ceil|round|abs|rand|sqrt|pow|length|min|max|average|sum|median|variance_population|" +
"variance_sample|first|last|unique|matches|merge|merge_recursive|has|attributes|values|unset|unset_recursive|keep|" +
"near|within|within_rectangle|is_in_polygon|fulltext|paths|traversal|traversal_tree|edges|stddev_sample|" + 
"stddev_population|slice|nth|position|translate|zip|call|apply|push|append|pop|shift|unshift|remove_value" + 
"remove_nth|graph_paths|shortest_path|graph_shortest_path|graph_distance_to|graph_traversal|graph_traversal_tree|" + 
"graph_edges|graph_vertices|neighbors|graph_neighbors|graph_common_neighbors|graph_common_properties|" +
"graph_eccentricity|graph_betweenness|graph_closeness|graph_absolute_eccentricity|remove_values|" +
"graph_absolute_betweenness|graph_absolute_closeness|graph_diameter|graph_radius|date_now|" +
"date_timestamp|date_iso8601|date_dayofweek|date_year|date_month|date_day|date_hour|" +
"date_minute|date_second|date_millisecond|date_dayofyear|date_isoweek|date_leapyear|date_quarter|date_days_in_month|" + 
"date_add|date_subtract|date_diff|date_compare|date_format|fail|passthru|sleep|not_null|" +
"first_list|first_document|parse_identifier|current_user|current_database|" +
"collections|document|union|union_distinct|intersection|flatten|" +
"ltrim|rtrim|find_first|find_last|split|substitute|md5|sha1|hash|random_token|AQL_LAST_ENTRY",

    listenKey: function(e) {
      if (e.keyCode === 27) {
        if (this.callbackSuccess) {
          this.callbackCancel();
        }
        this.hide();
      }
      else if (e.keyCode === 13) {
        if (this.callbackSuccess) {
          this.callbackSuccess($(this.typeahead).val());
          this.hide();
        }
      }
    },

    substringMatcher: function(strs) {
      return function findMatches(q, cb) {
        var matches, substrRegex;

        matches = [];

        substrRegex = new RegExp(q, 'i');

        _.each(strs, function(str) {
          if (substrRegex.test(str)) {
            matches.push(str);
          }
        });

        cb(matches);
      };
    },

    updateDatasets: function() {
      var self = this;
      this.collections = {
        system: [],
        doc: [],
        edge: [],
      };

      window.App.arangoCollectionsStore.each(function(collection) {
        if (collection.get("isSystem")) {
          self.collections.system.push(collection.get("name"));
        }
        else if (collection.get("type") === 'document') {
          self.collections.doc.push(collection.get("name"));
        }
        else {
          self.collections.edge.push(collection.get("name"));
        }
      });
    },

    stringToArray: function() {
      var self = this;

      _.each(this.aqlKeywords.split('|'), function(value) {
        self.aqlKeywordsArray.push(value.toUpperCase());
      });
      _.each(this.aqlBuiltinFunctions.split('|'), function(value) {
        self.aqlBuiltinFunctionsArray.push(value.toUpperCase());
      });

      //special case for keywords
      self.aqlKeywordsArray.push(true);
      self.aqlKeywordsArray.push(false);
      self.aqlKeywordsArray.push(null);
    },

    show: function(callbackSuccess, callbackCancel, type) {

      this.callbackSuccess = callbackSuccess;
      this.callbackCancel = callbackCancel;
      this.stringToArray();
      this.updateDatasets();

      var genHeader = function(name, icon, type) {
        var string = '<div class="header-type"><h4>' + name + '</h4>';
        if (icon) {
          string += '<span><i class="fa ' + icon + '"></i></span>';
        }
        if (type) {
          string += '<span class="type">' + type.toUpperCase() + '</span>';
        }
        string += '</div>';

        return string;
      };

      $(this.el).html(this.template.render({}));
      $(this.el).show();

      if (type === 'aql') {
        this.typeahead = $('#spotlight .typeahead').typeahead(
          {
            hint: true,
            highlight: true,
            minLength: 1
          },
          {
            name: 'Functions',
            source: this.substringMatcher(this.aqlBuiltinFunctionsArray),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Functions", "fa-code", "aql")
            }
          },
          {
            name: 'Keywords',
            source: this.substringMatcher(this.aqlKeywordsArray),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Keywords", "fa-code", "aql")
            }
          },
          {
            name: 'Documents',
            source: this.substringMatcher(this.collections.doc),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Documents", "fa-file-text-o", "Collection")
            }
          },
          {
            name: 'Edges',
            source: this.substringMatcher(this.collections.edge),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Edges", "fa-share-alt", "Collection")
            }
          },
          {
            name: 'System',
            limit: this.displayLimit,
            source: this.substringMatcher(this.collections.system),
            templates: {
              header: genHeader("System", "fa-cogs", "Collection")
            }
          }
        );
      }
      else {
        this.typeahead = $('#spotlight .typeahead').typeahead(
          {
            hint: true,
            highlight: true,
            minLength: 1
          },
          {
            name: 'Documents',
            source: this.substringMatcher(this.collections.doc),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Documents", "fa-file-text-o", "Collection")
            }
          },
          {
            name: 'Edges',
            source: this.substringMatcher(this.collections.edge),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Edges", "fa-share-alt", "Collection")
            }
          },
          {
            name: 'System',
            limit: this.displayLimit,
            source: this.substringMatcher(this.collections.system),
            templates: {
              header: genHeader("System", "fa-cogs", "Collection")
            }
          }
        );
      }
      

      $('#spotlight .typeahead').focus();
    },

    hide: function() {
      $(this.el).hide();
      this.typeahead = $('#spotlight .typeahead').typeahead('destroy');
    }

  });
}());
