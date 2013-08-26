/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog*/

window.arangoLogs = Backbone.Collection.extend({
  url: '/_admin/log?upto=4&size=10&offset=0',
  parse: function(response)  {
    var myResponse = [];
    var i = 0;
    $.each(response.lid, function(key, val) {
      myResponse.push({
        "level":response.level[i],
        "lid":response.lid[i],
        "text":response.text[i],
        "timestamp":response.timestamp[i],
        "totalAmount":response.totalAmount
      });
      i++;
    });
    return myResponse;
  },
  tables: ["logTableID", "warnTableID", "infoTableID", "debugTableID", "critTableID"],
  model: arangoLog,
  clearLocalStorage: function () {
    window.arangoLogsStore.reset();
  },
  returnElements: function () {
  },
  fillLocalStorage: function (table, offset, size) {
    var self = this;
    this.clearLocalStorage();
    if (!table) {
      table = 'logTableID';
    }
    if (!size) {
      size = 10;
    }
    if (!offset) {
      offset = 0;
    }

    var loglevel = this.showLogLevel(table);
    var url = "";
    if (loglevel === 5) {
      url = "/_admin/log?upto=4&size="+size+"&offset="+offset;
    }
    else {
      url = "/_admin/log?level="+loglevel+"&size="+size+"$offset="+offset;
    }
    $.getJSON(url, function(response) {
      var totalAmount = response.totalAmount;
      var i=0;
      $.each(response.lid, function () {
        window.arangoLogsStore.add({
          "level":response.level[i],
          "lid":response.lid[i],
          "text":response.text[i],
          "timestamp":response.timestamp[i],
          "totalAmount":response.totalAmount
        });
        i++;
      });
      window.logsView.drawTable();
    });
  },
  showLogLevel: function (tableid) {
    tableid = '#'+tableid;
    var returnVal = 0;
    if (tableid === "#critTableID") {
      returnVal = 1;
    }
    else if (tableid === "#warnTableID") {
      returnVal = 2;
    }
    else if (tableid === "#infoTableID") {
      returnVal = 3;
    }
    else if (tableid === "#debugTableID") {
      returnVal = 4;
    }
    else if (tableid === "#logTableID") {
      returnVal = 5;
    }
    return returnVal;
  }
});
