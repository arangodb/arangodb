/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, $, _ */
(function () {

  "use strict";

  window.ArangoLogs = window.PaginatedCollection.extend({
    upto: false,
    loglevel: 0,

    parse: function(response) {
      var myResponse = [];
      _.each(response.lid, function(val, i) {
        myResponse.push({
          level: response.level[i],
          lid: val,
          text: response.text[i],
          timestamp: response.timestamp[i],
          totalAmount: response.totalAmount
        });
      });
      this.totalAmount = response.totalAmount;
      return myResponse;
    },

    initialize: function(options) {
      if (options.upto === true) {
        this.upto = true;
      }
      this.loglevel = options.loglevel;
    },

    model: window.newArangoLog,

    url: function() {
      var type, rtnStr, offset, size;
      offset = this.page * this.pagesize;
      var inverseOffset = this.totalAmount % this.pagesize - (this.pagesize * this.page);
      if (inverseOffset < 0) {
        inverseOffset = 0;
        size = (this.totalAmount % this.pagesize);
      }
      else {
        size = this.pagesize;
      }

      if (this.totalAmount === 0) {
        size = 1;
      }

      if (this.upto) {
        type = 'upto';
      }
      else  {
        type = 'level';
      }
      rtnStr = '/_admin/log?'+type+'='+this.loglevel+'&size='+size+'&offset='+inverseOffset;
      return rtnStr;
    }

  });
}());
