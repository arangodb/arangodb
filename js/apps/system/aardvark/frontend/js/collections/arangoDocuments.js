/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
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

    setCollection: function(id) {
      this.collectionID = id;
    },

    // TODO Remove this block

    getFirstDocuments: function () {
      this.setToFirst();
    },

    getLastDocuments: function () {
      this.setToLast();
    },

    getPrevDocuments: function () {
      this.setToPrev();
    },
    getNextDocuments: function () {
      this.setToNext();
    },

    // TODO Endof Remove this block

    getTotalDocuments: function() {
      var result;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          result = data.count;
        },
        error: function() {
        }
      });
      this.setTotal(result);
      return result;
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
        return;
      }
      var query = " FILTER",
      parts = _.map(this.filters, function(f, i) {
        var res = " x.`";
        res += f.attr;
        res += "` ";
        res += f.op;
        res += " @param";
        res += i;
        bindVars["param" + i] = this.val;
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
          bindVars;
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

      var myQuery = {
        query: query,
        bindVars: bindVars
      };

      $.ajax({
        cache: false,
        type: 'POST',
        async: false,
        url: '/_api/cursor',
        data: JSON.stringify(myQuery),
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

    getFilteredDocuments: function (colid, currpage, filter, bindValues) {
      var self = this;
      this.collectionID = colid;
      this.currentPage = currpage;
      this.currentFilterPage = currpage;
      var filterString;
      if(filter.length === 0){
        filterString ="";
      } else {
        filterString = ' FILTER' + filter.join(' && ');
      }

      var sortCount = 10000;

      var sortString = '';
      if (this.documentsCount <= sortCount) {
        //sorted
        sortString = " SORT TO_NUMBER(u._key) == 0 ? u._key : TO_NUMBER(u._key)";
      }

      var myQueryVal = "FOR u in @@collection" + filterString + sortString +
                       " LIMIT @offset, @count RETURN u";

      this.offset = (this.currentPage - 1) * this.documentsPerPage;

      var myQuery = {
        query: myQueryVal,
        bindVars: {
          "@collection": this.collectionID,
          "count": this.documentsPerPage,
          "offset": this.offset
        },
        options: {
          fullCount: true
        }
      };

      $.each(bindValues, function(k,v) {
        myQuery.bindVars[k] = v;
      });

      $.ajax({
        cache: false,
        type: 'POST',
        async: false,
        url: '/_api/cursor',
        data: JSON.stringify(myQuery),
        contentType: "application/json",
        success: function(data) {
          self.clearDocuments();
          self.documentsCount = data.extra.fullCount;
          self.totalPages = Math.ceil(self.documentsCount / self.documentsPerPage);
          if (
            isNaN(this.currentPage)
            || this.currentPage === undefined
            || this.currentPage < 1
          ) {
            this.currentPage = 1;
          }
          if (this.totalPages === 0 || this.totalPages === undefined) {
            this.totalPages = 1;
          }

          this.offset = (this.currentPage - 1) * this.documentsPerPage;
          if (self.documentsCount !== 0) {
            $.each(data.result, function(k, v) {
              window.arangoDocumentsStore.add({
                "id": v._id,
                "rev": v._rev,
                "key": v._key,
                "content": v
              });
            });
            window.documentsView.drawTable();
            window.documentsView.renderPagination(self.totalPages, true);
          }
          else {
            window.documentsView.initTable();
            window.documentsView.drawTable();
          }
        },
        error: function(data) {
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    getStatisticsHistory: function(params) {
      var self = this;
      var body = {
        startDate : params.startDate,
        endDate   : params.endDate,
        figures   : params.figures
      };
      var server = params.server;
      var addAuth = function(){};
      var url = "";
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
