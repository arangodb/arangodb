/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, window, $, _ */
(function () {

  "use strict";

  window.PaginatedCollection = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    totalAmount: 0,

    getPage: function() {
      return this.page + 1;
    },

    setPage: function(counter) {
      this.page = counter-1;
    },

    getLastPageNumber: function() {
      return Math.ceil(this.totalAmount / this.pagesize);
    }

  });
}());
