/* jshint browser: true */
/* jshint unused: false */
/* global window, _, arangoHelper */
(function () {
  'use strict';

  window.ArangoLogs = window.PaginatedCollection.extend({
    upto: false,
    loglevel: 0,
    totalPages: 0,

    parse: function (response) {
      var myResponse = [];
      _.each(response.lid, function (val, i) {
        myResponse.push({
          level: response.level[i],
          lid: val,
          topic: response.topic[i],
          text: response.text[i],
          timestamp: response.timestamp[i],
          totalAmount: response.totalAmount
        });
      });
      this.totalAmount = response.totalAmount;
      this.totalPages = Math.ceil(this.totalAmount / this.pagesize);
      return myResponse;
    },

    initialize: function (options) {
      if (options.upto === true) {
        this.upto = true;
      }
      this.loglevel = options.loglevel;
    },

    model: window.newArangoLog,

    url: function () {
      var type; var rtnStr; var size;

      var inverseOffset = this.totalAmount - ((this.page + 1) * this.pagesize);
      if (inverseOffset < 0 && this.page === (this.totalPages - 1)) {
        inverseOffset = 0;
        size = (this.totalAmount % this.pagesize);
      } else {
        size = this.pagesize;
      }

      // if totalAmount (first fetch) = 0, then set size to 1 (reduce traffic)
      if (this.totalAmount === 0) {
        size = 1;
      }

      if (this.upto) {
        type = 'upto';
      } else {
        type = 'level';
      }
      rtnStr = '/_admin/log?' + type + '=' + this.loglevel + '&size=' + size + '&offset=' + inverseOffset;
      this.lastInverseOffset = inverseOffset;
      return arangoHelper.databaseUrl(rtnStr);
    }

  });
}());
