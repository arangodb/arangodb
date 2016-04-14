/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.IndicesView = Backbone.View.extend({

    el: "#content",

    initialize: function(options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
    },

    render: function() {
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Indices');

    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    }

  });

}());
