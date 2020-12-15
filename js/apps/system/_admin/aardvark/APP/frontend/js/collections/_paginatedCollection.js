/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, window */
(function () {
  'use strict';

  window.PaginatedCollection = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    totalAmount: null,

    getPage: function () {
      return this.page + 1;
    },

    setPage: function (counter) {
      if (counter >= this.getLastPageNumber() && this.totalAmount !== null) {
        this.page = this.getLastPageNumber() - 1;
        return;
      }
      if (counter < 1) {
        this.page = 0;
        return;
      }
      this.page = counter - 1;
    },

    getLastPageNumber: function () {
      return Math.max(Math.ceil(this.totalAmount / this.pagesize), 1);
    },

    getOffset: function () {
      return this.page * this.pagesize;
    },

    getPageSize: function () {
      return this.pagesize;
    },

    setPageSize: function (newPagesize) {
      if (newPagesize === 'all') {
        this.pagesize = 'all';
      } else {
        try {
          newPagesize = parseInt(newPagesize, 10);
          this.pagesize = newPagesize;
        } catch (ignore) {}
      }
    },

    setToFirst: function () {
      this.page = 0;
    },

    setToLast: function () {
      this.setPage(this.getLastPageNumber());
    },

    setToPrev: function () {
      this.setPage(this.getPage() - 1);
    },

    setToNext: function () {
      this.setPage(this.getPage() + 1);
    },

    setTotal: function (total) {
      this.totalAmount = total;
    },

    getTotal: function () {
      return this.totalAmount;
    },

    setTotalMinusOne: function () {
      this.totalAmount--;
    }

  });
}());
