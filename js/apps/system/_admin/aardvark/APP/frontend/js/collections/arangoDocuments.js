/* jshint browser: true */
/* jshint unused: false */
/* global window, _, arangoHelper, $ */
(function () {
  'use strict';

  window.ArangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],
    checkCursorTimer: undefined,

    MAX_SORT: 12000,

    lastQuery: {},
    sortAttribute: '',

    url: arangoHelper.databaseUrl('/_api/documents'),
    model: window.arangoDocumentModel,

    loadTotal: function (callback) {
      var self = this;
      $.ajax({
        cache: false,
        type: 'GET',
        url: arangoHelper.databaseUrl('/_api/collection/' + this.collectionID + '/count'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          self.setTotal(data.count);
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
    },

    setCollection: function (id, page) {
      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('Documents', 'Could not fetch documents count');
        }
      };
      this.resetFilter();
      this.collectionID = id;
      if (page) {
        this.setPage(page);
      } else {
        this.setPage(1);
      }
      this.loadTotal(callback);
    },

    setSort: function (key) {
      this.sortAttribute = key;
    },

    getSort: function () {
      return this.sortAttribute;
    },

    addFilter: function (attr, op, val) {
      this.filters.push({
        attr: attr,
        op: op,
        val: val
      });
    },

    setFiltersForQuery: function (bindVars) {
      if (this.filters.length === 0) {
        return '';
      }
      var parts = _.map(this.filters, function (f, i) {
        var res = 'x.@attr' + i + ' ' + f.op + ' @param' + i;

        if (f.op === 'LIKE') {
          bindVars['param' + i] = '%' + f.val + '%';
        } else if (f.op === 'IN' || f.op === 'NOT IN ') {
          if (f.val.indexOf(',') !== -1) {
            bindVars['param' + i] = f.val.split(',').map(function (v) { return v.replace(/(^ +| +$)/g, ''); });
          } else {
            bindVars['param' + i] = [ f.val ];
          }
        } else {
          bindVars['param' + i] = f.val;
        }

        if (f.attr.indexOf('.') !== -1) {
          bindVars['attr' + i] = f.attr.split('.');
        } else {
          bindVars['attr' + i] = f.attr;
        }

        return res;
      });
      return ' FILTER ' + parts.join(' && ');
    },

    setPagesize: function (size) {
      this.setPageSize(size);
    },

    resetFilter: function () {
      this.filters = [];
    },

    moveDocument: function (key, fromCollection, toCollection, callback) {
      var querySave;
      var queryRemove;
      var bindVars = {
        '@collection': fromCollection,
        'filterid': key
      };
      var queryObj1;
      var queryObj2;

      querySave = 'FOR x IN @@collection';
      querySave += ' FILTER x._key == @filterid';
      querySave += ' INSERT x IN ';
      querySave += toCollection;

      queryRemove = 'FOR x in @@collection';
      queryRemove += ' FILTER x._key == @filterid';
      queryRemove += ' REMOVE x IN @@collection';

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
        url: arangoHelper.databaseUrl('/_api/cursor'),
        data: JSON.stringify(queryObj1),
        contentType: 'application/json',
        success: function () {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            url: arangoHelper.databaseUrl('/_api/cursor'),
            data: JSON.stringify(queryObj2),
            contentType: 'application/json',
            success: function () {
              var error = false;
              if (callback) {
                callback(error);
              }
              window.progressView.hide();
            },
            error: function () {
              var error = true;
              if (callback) {
                callback(error);
              }
              window.progressView.hide();
              arangoHelper.arangoError(
                'Document error', 'Documents inserted, but could not be removed.'
              );
            }
          });
        },
        error: function () {
          window.progressView.hide();
          arangoHelper.arangoError('Document error', 'Could not move selected documents.');
        }
      });
    },

    getDocuments: function (callback) {
      var self = this;
      var query;
      var bindVars;
      var tmp;
      var queryObj;

      var pageSize = this.getPageSize();
      if (pageSize === 'all') {
        pageSize = this.MAX_SORT + 38000; // will result in 50k docs
      }

      bindVars = {
        '@collection': this.collectionID,
        'offset': this.getOffset(),
        'count': pageSize
      };

      // fetch just the first 25 attributes of the document
      // this number is arbitrary, but may reduce HTTP traffic a bit
      query = 'FOR x IN @@collection LET att = APPEND(SLICE(ATTRIBUTES(x), 0, 25), "_key", true)';
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        if (this.getSort() === '_key') {
          query += ' SORT TO_NUMBER(x.' + this.getSort() + ') == 0 ? x.' +
            this.getSort() + ' : TO_NUMBER(x.' + this.getSort() + ')';
        } else if (this.getSort() !== '') {
          query += ' SORT x.' + this.getSort();
        }
      }

      if (bindVars.count !== 'all') {
        query += ' LIMIT @offset, @count RETURN KEEP(x, att)';
      } else {
        tmp = {
          '@collection': this.collectionID
        };
        bindVars = tmp;
        query += ' RETURN KEEP(x, att)';
      }

      queryObj = {
        query: query,
        bindVars: bindVars,
        batchSize: pageSize
      };

      if (this.filters.length > 0) {
        queryObj.options = {
          fullCount: true
        };
      }

      var checkCursorStatus = function (jobid) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: arangoHelper.databaseUrl('/_api/job/' + encodeURIComponent(jobid)),
          contentType: 'application/json',
          success: function (data, textStatus, xhr) {
            if (xhr.status === 201) {
              window.progressView.toShow = false;
              self.clearDocuments();
              if (data.extra && data.extra.stats && data.extra.stats.fullCount !== undefined) {
                self.setTotal(data.extra.stats.fullCount);
              }
              if (self.getTotal() !== 0) {
                _.each(data.result, function (v) {
                  self.add({
                    'id': v._id,
                    'rev': v._rev,
                    'key': v._key,
                    'content': v
                  });
                });
              }
              self.lastQuery = queryObj;

              callback(false, data);
            } else if (xhr.status === 204) {
              self.checkCursorTimer = window.setTimeout(function () {
                checkCursorStatus(jobid);
              }, 500);
            }
          },
          error: function (data) {
            callback(false, data);
          }
        });
      };

      $.ajax({
        cache: false,
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/cursor'),
        data: JSON.stringify(queryObj),
        headers: {
          'x-arango-async': 'store'
        },
        contentType: 'application/json',
        success: function (data, textStatus, xhr) {
          if (xhr.getResponseHeader('x-arango-async-id')) {
            var jobid = xhr.getResponseHeader('x-arango-async-id');

            var cancelRunningCursor = function () {
              $.ajax({
                url: arangoHelper.databaseUrl('/_api/job/' + encodeURIComponent(jobid) + '/cancel'),
                type: 'PUT',
                success: function () {
                  window.clearTimeout(self.checkCursorTimer);
                  arangoHelper.arangoNotification('Documents', 'Canceled operation.');
                  $('.dataTables_empty').text('Canceled.');
                  window.progressView.hide();
                }
              });
            };

            window.progressView.showWithDelay(1000, 'Fetching documents...', cancelRunningCursor);

            checkCursorStatus(jobid);
          } else {
            callback(true, data);
          }
        },
        error: function (data) {
          callback(false, data);
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function () {
      var query, queryObj, bindVars;

      bindVars = {
        '@collection': this.collectionID
      };

      query = 'FOR x in @@collection';
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT && this.getSort().length > 0) {
        query += ' SORT x.' + this.getSort();
      }

      query += ' RETURN x';

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments: function (file, callback) {
      $.ajax({
        type: 'POST',
        url: arangoHelper.databaseUrl('/_api/import?type=auto&collection=' +
          encodeURIComponent(this.collectionID) +
          '&createCollection=false'),
        data: file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function (xhr) {
          if (xhr.readyState === 4 && xhr.status === 201) {
            callback(false);
          } else {
            try {
              var data = JSON.parse(xhr.responseText);
              if (data.errors > 0) {
                var result = 'At least one error occurred during upload';
                callback(false, result);
              }
            } catch (err) {
              console.log(err);
            }
          }
        },
        error: function (msg) {
          callback(true, msg.responseJSON.errorMessage);
        }
      });
    }
  });
}());
