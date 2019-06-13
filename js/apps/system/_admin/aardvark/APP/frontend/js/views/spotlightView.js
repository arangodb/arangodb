/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window, _ */
/* global arangoHelper, templateEngine */

(function () {
  'use strict';

  window.SpotlightView = Backbone.View.extend({
    template: templateEngine.createTemplate('spotlightView.ejs'),

    el: '#spotlightPlaceholder',

    displayLimit: 8,
    typeahead: null,
    callbackSuccess: null,
    callbackCancel: null,

    collections: {
      system: [],
      doc: [],
      edge: []
    },

    events: {
      'focusout #spotlight .tt-input': 'hide',
      'keyup #spotlight .typeahead': 'listenKey'
    },

    aqlKeywordsArray: [],
    aqlBuiltinFunctionsArray: [],

    aqlKeywords: 'for|return|filter|search|sort|limit|let|collect|asc|desc|in|into|' +
      'insert|update|remove|replace|upsert|options|with|and|or|not|' +
      'distinct|graph|shortest_path|outbound|inbound|any|all|none|aggregate|like|count',

    hide: function () {
      this.typeahead = $('#spotlight .typeahead').typeahead('destroy');
      $(this.el).hide();
    },

    listenKey: function (e) {
      if (e.keyCode === 27) {
        if (this.callbackSuccess) {
          this.callbackCancel();
        }
        this.hide();
      } else if (e.keyCode === 13) {
        if (this.callbackSuccess) {
          var string = $(this.typeahead).val();
          this.callbackSuccess(string);
          this.hide();
        }
      }
    },

    substringMatcher: function (strs) {
      return function findMatches (q, cb) {
        var matches, substrRegex;

        matches = [];

        substrRegex = new RegExp(q, 'i');

        _.each(strs, function (str) {
          if (substrRegex.test(str)) {
            matches.push(str);
          }
        });

        cb(matches);
      };
    },

    updateDatasets: function () {
      var self = this;
      this.collections = {
        system: [],
        doc: [],
        edge: []
      };

      window.App.arangoCollectionsStore.each(function (collection) {
        if (collection.get('isSystem')) {
          self.collections.system.push(collection.get('name'));
        } else if (collection.get('type') === 'document') {
          self.collections.doc.push(collection.get('name'));
        } else {
          self.collections.edge.push(collection.get('name'));
        }
      });
    },

    stringToArray: function () {
      var self = this;

      _.each(this.aqlKeywords.split('|'), function (value) {
        self.aqlKeywordsArray.push(value.toUpperCase());
      });

      // special case for keywords
      self.aqlKeywordsArray.push(true);
      self.aqlKeywordsArray.push(false);
      self.aqlKeywordsArray.push(null);
    },

    fetchKeywords: function (callback) {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/aql-builtin'),
        contentType: 'application/json',
        success: function (data) {
          self.stringToArray();
          self.updateDatasets();
          _.each(data.functions, function (val) {
            self.aqlBuiltinFunctionsArray.push(val.name);
          });
          if (callback) {
            callback();
          }
        },
        error: function () {
          if (callback) {
            callback();
          }
          arangoHelper.arangoError('AQL', 'Could not fetch AQL function definition.');
        }
      });
    },

    show: function (callbackSuccess, callbackCancel, type) {
      var self = this;

      this.callbackSuccess = callbackSuccess;
      this.callbackCancel = callbackCancel;

      var continueRender = function () {
        var genHeader = function (name, icon, type) {
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
              source: self.substringMatcher(self.aqlBuiltinFunctionsArray),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Functions', 'fa-code', 'aql')
              }
            },
            {
              name: 'Keywords',
              source: self.substringMatcher(self.aqlKeywordsArray),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Keywords', 'fa-code', 'aql')
              }
            },
            {
              name: 'Documents',
              source: self.substringMatcher(self.collections.doc),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Documents', 'fa-file-text-o', 'Collection')
              }
            },
            {
              name: 'Edges',
              source: self.substringMatcher(self.collections.edge),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Edges', 'fa-share-alt', 'Collection')
              }
            },
            {
              name: 'System',
              limit: self.displayLimit,
              source: self.substringMatcher(self.collections.system),
              templates: {
                header: genHeader('System', 'fa-cogs', 'Collection')
              }
            }
          );
        } else {
          this.typeahead = $('#spotlight .typeahead').typeahead(
            {
              hint: true,
              highlight: true,
              minLength: 1
            },
            {
              name: 'Documents',
              source: self.substringMatcher(self.collections.doc),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Documents', 'fa-file-text-o', 'Collection')
              }
            },
            {
              name: 'Edges',
              source: self.substringMatcher(self.collections.edge),
              limit: self.displayLimit,
              templates: {
                header: genHeader('Edges', 'fa-share-alt', 'Collection')
              }
            },
            {
              name: 'System',
              limit: self.displayLimit,
              source: self.substringMatcher(self.collections.system),
              templates: {
                header: genHeader('System', 'fa-cogs', 'Collection')
              }
            }
          );
        }

        $('#spotlight .typeahead').focus();
      }.bind(this);

      if (self.aqlBuiltinFunctionsArray.length === 0) {
        this.fetchKeywords(continueRender);
      } else {
        continueRender();
      }
    }
  });
}());
