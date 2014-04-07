/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global require, exports, window, Backbone, arangoDocumentModel, _, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    currentPage: 1,
    collectionID: 1,
    totalPages: 1,
    documentsPerPage: 10,
    documentsCount: 1,
    offset: 0,

    filters: [],

    url: '/_api/documents',
    model: window.arangoDocumentModel,

    loadTotal: function() {
      var self = this;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          self.setTotal(data.count);
        }
      });
    },

    setCollection: function(id) {
      this.resetFilter();
      this.collectionID = id;
      this.setPage(1);
      this.loadTotal();
    },

    addFilter: function(attr, op, val) {
      this.filters.push({
        attr: attr,
        op: op,
        val: val
      });
    },

    setFiltersForQuery: function(bindVars) {
      if (this.filters.length === 0) {
        return "";
      }
      var query = " FILTER",
      parts = _.map(this.filters, function(f, i) {
        var res = " x.`";
        res += f.attr;
        res += "` ";
        res += f.op;
        res += " @param";
        res += i;
        bindVars["param" + i] = f.val;
        return res;
      });
      return query + parts.join(" &&");
    },

    resetFilter: function() {
      this.filters = [];
    },

    getDocuments: function () {
      var self = this,
          query,
          bindVars,
          queryObj;
      bindVars = {
        "@collection": this.collectionID,
        "offset": this.getOffset(),
        "count": this.getPageSize()
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < 10000) {
        query += " SORT TO_NUMBER(x._key) == 0 ? x._key : TO_NUMBER(x._key)";
      }
      query += " LIMIT @offset, @count RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      $.ajax({
        cache: false,
        type: 'POST',
        async: false,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        contentType: "application/json",
        success: function(data) {
          self.clearDocuments();
          self.setTotal(data.extra.fullCount);
          if (self.documentsCount !== 0) {
            _.each(data.result, function(v) {
              self.add({
                "id": v._id,
                "rev": v._rev,
                "key": v._key,
                "content": v
              });
            });
          }
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    getStatisticsHistory: function(params) {
      var self = this,
      body = {
        startDate : params.startDate,
        endDate   : params.endDate,
        figures   : params.figures
      },
      server = params.server,
      addAuth = function(){},
      url = "";
      if (server) {
        url = server.endpoint;
        url += "/_admin/history";
        if (server.isDBServer) {
          url += "?DBserver=" + server.target;
        }
        addAuth = server.addAuth;
      } else {
        url = "/_admin/history";
      }
      $.ajax({
        cache: false,
        type: 'POST',
        async: false,
        url: url,
        data: JSON.stringify(body),
        contentType: "application/json",
        beforeSend: addAuth,
        success: function(data) {
          self.history =  data.result;
        },
        error: function() {

        }
      });
    },

    updloadDocuments : function (file) {
      var result;
      $.ajax({
        type: "POST",
        async: false,
        url:
        '/_api/import?type=auto&collection='+
        encodeURIComponent(this.collectionID)+
        '&createCollection=false',
        data: file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function(xhr) {
          if (xhr.readyState === 4 && xhr.status === 201) {
            result =  xhr.responseText;
            try {
              result =  true;
            } catch (e) {
              result =  "Error: " + e;
            }
          } else {
            result =  "Upload error";
          }
        }
      });
      return result;
    }
  });
}());
