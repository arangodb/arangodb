/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, window, Backbone, arangoDocumentModel, _, $*/
(function() {
  "use strict";

  window.arangoDocuments = Backbone.Collection.extend({
        currentPage: 1,
        collectionID: 1,
        totalPages: 1,
        documentsPerPage: 10,
        documentsCount: 1,
        offset: 0,

        url: '/_api/documents',
        model: arangoDocumentModel,
        getFirstDocuments: function () {
          if (this.currentPage !== 1) {
            var link = window.location.hash.split("/");
            window.location.hash = link[0] + "/" + link[1] + "/" + link[2] + "/1";
          }
        },
        getLastDocuments: function () {
          if (this.currentPage !== this.totalPages) {
            var link = window.location.hash.split("/");
            window.location.hash = link[0] + "/" + link[1] + "/" + link[2] + "/" + this.totalPages;
          }
        },
        getPrevDocuments: function () {
          if (this.currentPage !== 1) {
            var link = window.location.hash.split("/");
            var page = parseInt(this.currentPage, null) - 1;
            window.location.hash = link[0] + "/" + link[1] + "/" + link[2] + "/" + page;
          }
        },
        getNextDocuments: function () {
          if (this.currentPage !== this.totalPages) {
            var link = window.location.hash.split("/");
            var page = parseInt(this.currentPage, null) + 1;
            window.location.hash = link[0] + "/" + link[1] + "/" + link[2] + "/" + page;
          }
        },
        getDocuments: function (colid, currpage) {
          var self = this;
          this.collectionID = colid;
          this.currentPage = currpage;

          $.ajax({
            cache: false,
            type: "GET",
            url: "/_api/collection/" + this.collectionID + "/count",
            contentType: "application/json",
            processData: false,
            async: false,
            success: function(data) {
              self.totalPages = Math.ceil(data.count / self.documentsPerPage);
              self.documentsCount = data.count;
            },
            error: function(data) {
            }
          });


          if (isNaN(this.currentPage) || this.currentPage === undefined || this.currentPage < 1) {
            this.currentPage = 1;
          }
          if (this.totalPages === 0) {
            this.totalPages = 1;
          }

          this.offset = (this.currentPage - 1) * this.documentsPerPage;

          var myQueryVal;
          var sortCount = 10000;

          if (this.documentsCount <= sortCount) {
            //sorted
            myQueryVal = "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " + 
                         "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x";
          }
          else {
            //not sorted
            myQueryVal = "FOR x in @@collection LIMIT @offset, @count RETURN x";
          }

          var myQuery = {
            query: myQueryVal,
            bindVars: {
              "@collection": this.collectionID,
              "offset": this.offset,
              "count": this.documentsPerPage
            }
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
              if (self.documentsCount !== 0) {
                $.each(data.result, function(k, v) {
                  //ERROR HERE
                  window.arangoDocumentsStore.add({
                    "id": v._id,
                    "rev": v._rev,
                    "key": v._key,
                    "content": v
                  });
                });
                window.documentsView.drawTable();
                window.documentsView.renderPagination(self.totalPages);
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
              if (this.totalPages === 0) {
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
          window.arangoDocumentsStore.reset();
        }
  });
}());
