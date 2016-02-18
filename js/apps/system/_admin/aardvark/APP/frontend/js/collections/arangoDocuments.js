/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, arangoDocumentModel, _, arangoHelper, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],
    checkCursorTimer: undefined,

    MAX_SORT: 12000,

    lastQuery: {},
    sortAttribute: "_key",

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

    setSort: function(key) {
      this.sortAttribute = key;
    },

    getSort: function() {
      return this.sortAttribute;
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
      var query = " FILTER", res = '',
      parts = _.map(this.filters, function(f, i) {
        if (f.op === 'LIKE') {
          res = " " + f.op + "(x.`" + f.attr + "`, @param";
          res += i;
          res += ")";
        }
        else {
          if (f.op === 'IN' || f.op === 'NOT IN') {
            res = ' ';
          }
          else {
            res = " x.`";
          }

          res += f.attr;

          if (f.op === 'IN' || f.op === 'NOT IN') {
            res += " ";
          }
          else {
            res += "` ";
          }

          res += f.op;

          if (f.op === 'IN' || f.op === 'NOT IN') {
            res += " x.@param";
          }
          else {
            res += " @param";
          }
          res += i;
        }

        bindVars["param" + i] = f.val;
        return res;
      });
      return query + parts.join(" &&");
    },

    setPagesize: function(size) {
      this.setPageSize(size);
    },

    resetFilter: function() {
      this.filters = [];
    },

    moveDocument: function (key, fromCollection, toCollection, callback) {
      var querySave, queryRemove, bindVars = {
        "@collection": fromCollection,
        "filterid": key
      }, queryObj1, queryObj2;

      querySave = "FOR x IN @@collection";
      querySave += " FILTER x._key == @filterid";
      querySave += " INSERT x IN ";
      querySave += toCollection;

      queryRemove = "FOR x in @@collection";
      queryRemove += " FILTER x._key == @filterid";
      queryRemove += " REMOVE x IN @@collection";

      queryObj1 = {
        query: querySave,
        bindVars: bindVars
      };

      queryObj2 = {
        query: queryRemove,
        bindVars: bindVars
      };

      window.progressView.show();
      // first insert docs in toCollection
      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj1),
        contentType: "application/json",
        success: function() {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            async: true,
            url: '/_api/cursor',
            data: JSON.stringify(queryObj2),
            contentType: "application/json",
            success: function() {
              if (callback) {
                callback();
              }
              window.progressView.hide();
            },
            error: function() {
              window.progressView.hide();
              arangoHelper.arangoError(
                "Document error", "Documents inserted, but could not be removed."
              );
            }
          });
        },
        error: function() {
          window.progressView.hide();
          arangoHelper.arangoError("Document error", "Could not move selected documents.");
        }
      });
    },

    getDocuments: function (callback) {
      var self = this,
          query,
          bindVars,
          tmp,
          queryObj;
      bindVars = {
        "@collection": this.collectionID,
        "offset": this.getOffset(),
        "count": this.getPageSize()
      };

      // fetch just the first 25 attributes of the document
      // this number is arbitrary, but may reduce HTTP traffic a bit
      query = "FOR x IN @@collection LET att = SLICE(ATTRIBUTES(x), 0, 25)";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        if (this.getSort() === '_key') {
          query += " SORT TO_NUMBER(x." + this.getSort() + ") == 0 ? x."
                + this.getSort() + " : TO_NUMBER(x." + this.getSort() + ")";
        }
        else {
          query += " SORT x." + this.getSort();
        }
      }

      if (bindVars.count !== 'all') {
        query += " LIMIT @offset, @count RETURN KEEP(x, att)";
      }
      else {
        tmp = {
          "@collection": this.collectionID
        };
        bindVars = tmp;
        query += " RETURN KEEP(x, att)";
      }

      queryObj = {
        query: query,
        bindVars: bindVars
      };
      if (this.getTotal() < 10000 || this.filters.length > 0) {
        queryObj.options = {
          fullCount: true,
        };
      }

      var checkCursorStatus = function(jobid) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: '/_api/job/' + encodeURIComponent(jobid),
          contentType: 'application/json',
          success: function(data, textStatus, xhr) {
            if (xhr.status === 201) {
              window.progressView.toShow = false;
              self.clearDocuments();
              if (data.extra && data.extra.stats.fullCount !== undefined) {
                self.setTotal(data.extra.stats.fullCount);
              }
              if (self.getTotal() !== 0) {
                _.each(data.result, function(v) {
                  self.add({
                    "id": v._id,
                    "rev": v._rev,
                    "key": v._key,
                    "content": v
                  });
                });
              }
              self.lastQuery = queryObj;

              callback(false, data);
            }
            else if (xhr.status === 204) {
              self.checkCursorTimer = window.setTimeout(function() {
                checkCursorStatus(jobid);
              }, 500);
            }

          },
          error: function(data) {
            callback(false, data);
          }
        });
      };

      $.ajax({
        cache: false,
        type: 'POST',
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        headers: {
          'x-arango-async': 'store'
        },
        contentType: "application/json",
        success: function (data, textStatus, xhr) {

          if (xhr.getResponseHeader('x-arango-async-id')) {
            var jobid = xhr.getResponseHeader('x-arango-async-id');

            var cancelRunningCursor = function() {
              $.ajax({
                url: '/_api/job/'+ encodeURIComponent(jobid) + "/cancel",
                type: 'PUT',
                success: function() {
                  window.clearTimeout(self.checkCursorTimer);
                  arangoHelper.arangoNotification("Documents", "Canceled operation.");
                  $('.dataTables_empty').text('Canceled.');
                  window.progressView.hide();
                }
              });
            };

            window.progressView.showWithDelay(300, "Fetching documents...", cancelRunningCursor);

            checkCursorStatus(jobid);
          }
          else {
            callback(true, data);
          }
        },
        error: function(data) {
          callback(false, data);
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function() {
      var query, queryObj, bindVars;

      bindVars = {
        "@collection": this.collectionID
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        query += " SORT x." + this.getSort();
      }

      query += " RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments : function (file) {
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
            result = true;
          } else {
            result = "Upload error";
          }

          try {
            var data = JSON.parse(xhr.responseText);
            if (data.errors > 0) {
              result = "At least one error occurred during upload";
            }
          }
          catch (err) {
            console.log(err);
          }               
        }
      });
      return result;
    }
  });
}());
