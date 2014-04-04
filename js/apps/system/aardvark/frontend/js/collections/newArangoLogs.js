/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
(function () {

  "use strict";

  window.NewArangoLogs = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    upto: false,
    loglevel: 0,

    parse: function(response) {
      var myResponse = [];
      var i = 0;
      $.each(response.lid, function(key, val) {
        myResponse.push({
          "level": response.level[i],
          "lid": response.lid[i],
          "text": response.text[i],
          "timestamp": response.timestamp[i],
          "totalAmount": response.totalAmount
        });
        i++;
      });
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
