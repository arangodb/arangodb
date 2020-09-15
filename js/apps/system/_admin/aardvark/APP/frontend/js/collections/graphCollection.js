/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,

    sortOptions: {
      desc: false
    },

    url: arangoHelper.databaseUrl('/_api/gharial'),

    dropAndDeleteGraph: function (name, callback) {
      $.ajax({
        type: 'DELETE',
        url: arangoHelper.databaseUrl('/_api/gharial/') + encodeURIComponent(name) + '?dropCollections=true',
        contentType: 'application/json',
        processData: true,
        success: function () {
          callback(true);
        },
        error: function (data) {
          if (data.responseJSON) {
            callback(false, data.responseJSON);
          } else {
            callback(false);
          }
        }
      });
    },

    createNode: function (gName, gCollection, data, callback) {
      $.ajax({
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/gharial/') + encodeURIComponent(gName) + '/vertex/' + encodeURIComponent(gCollection),
        contentType: 'application/json',
        data: JSON.stringify(data),
        processData: true,
        success: function (response) {
          callback(false, response.vertex._id);
        },
        error: function (response) {
          callback(true, null, response.responseJSON.errorMessage);
        }
      });
    },

    createEdge: function (gName, gCollection, data, callback) {
      $.ajax({
        cache: false,
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/gharial/') + encodeURIComponent(gName) + '/edge/' + encodeURIComponent(gCollection),
        data: JSON.stringify(data),
        contentType: 'application/json',
        processData: false,
        success: function (response) {
          callback(false, response.edge._id);
        },
        error: function (response) {
          callback(true, null, response.responseJSON.errorMessage);
        }
      });
    },

    comparator: function (item, item2) {
      var a = item.get('_key') || '';
      var b = item2.get('_key') || '';
      a = a.toLowerCase();
      b = b.toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function (val) {
      this.sortOptions.desc = val;
    },

    parse: function (res) {
      if (!res.error) {
        return res.graphs;
      }
    }
  });
}());
