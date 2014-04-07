/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, $, _ */
(function () {

  "use strict";

  window.NewArangoLogs = window.PaginatedCollection.extend({
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
      var type, rtnStr, offset;
      offset = this.page * this.pagesize;

      if (this.upto) {
        type = 'upto';
      }
      else  {
        type = 'level';
      }
      rtnStr = '/_admin/log?'+type+'='+this.loglevel+'&size='+this.pagesize+'&offset='+offset;
      return rtnStr;
    }

  });
}());
