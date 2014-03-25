/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("arangoDocuments", function() {
    
    var col;

    beforeEach(function() {
        window.documentsView = new window.DocumentsView();
        col = new window.arangoDocuments();
        window.arangoDocumentsStore = col;
    });

      it("should getFirstDocuments", function() {
          col.currentPage = 2;
          window.location.hash = "a/b/c"
          col.getFirstDocuments();
          expect(window.location.hash).toEqual("#a/b/c/1");
      });

      it("should getLastDocuments", function() {
          col.currentPage = 2;
          col.totalPages = 5;
          window.location.hash = "a/b/c"
          col.getLastDocuments();
          expect(window.location.hash).toEqual("#a/b/c/5");
      });
      it("should getPrevDocuments", function() {
          col.currentPage = 2;
          window.location.hash = "a/b/c"
          col.getPrevDocuments();
          expect(window.location.hash).toEqual("#a/b/c/1");
      });

      it("should getNextDocuments", function() {
          col.currentPage = 2;
          col.totalPages = 5;
          window.location.hash = "a/b/c"
          col.getNextDocuments();
          expect(window.location.hash).toEqual("#a/b/c/3");
      });

      it("should getDocuments and succeed", function() {
          var colid = "12345";
          var currpage = "0";
          spyOn($, "ajax").andCallFake(function(opt) {
              if (opt.type === "GET") {
                  expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.processData).toEqual(false);
                  opt.success({count : 100});
              } else if (opt.type === "POST") {
                  expect(opt.url).toEqual('/_api/cursor');
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.data).toEqual(JSON.stringify({
                      query : "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " +
                      "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x",
                      bindVars : {
                        "@collection" : colid,
                        "offset": 0,
                        "count": 10
                      }
                  }));
                  opt.success({result : [{_id : 1, _rev : 2, _key : 4}, {_id : 2, _rev : 2, _key : 4},
                      {_id : 3, _rev : 2, _key : 4}]
                  });
              }
          });
          spyOn(window.arangoDocumentsStore, "reset");
          spyOn(window.documentsView, "drawTable");
          spyOn(window.documentsView, "renderPagination");
          spyOn(window.documentsView, "initTable");
          var result = col.getDocuments(colid, currpage);
          expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10);
      });

      it("should getDocuments with exceeding sort count", function() {
          var colid = "12345";
          var currpage = "2";
          spyOn($, "ajax").andCallFake(function(opt) {
              if (opt.type === "GET") {
                  expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.processData).toEqual(false);
                  opt.success({count : 100000});
              } else if (opt.type === "POST") {
                  expect(opt.url).toEqual('/_api/cursor');
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.data).toEqual(JSON.stringify({
                      query : "FOR x in @@collection LIMIT @offset, @count RETURN x",
                      bindVars : {
                          "@collection" : colid,
                          "offset": 10,
                          "count": 10
                      }
                  }));
                  opt.success({result : [{_id : 1, _rev : 2, _key : 4}, {_id : 2, _rev : 2, _key : 4},
                      {_id : 3, _rev : 2, _key : 4}]
                  });
              }
          });
          spyOn(window.arangoDocumentsStore, "reset");
          spyOn(window.documentsView, "drawTable");
          spyOn(window.documentsView, "renderPagination");
          spyOn(window.documentsView, "initTable");
          var result = col.getDocuments(colid, currpage);
          expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10000);
      });

      it("should getDocuments with initial draw", function() {
          var colid = "12345";
          var currpage = "2";
          spyOn($, "ajax").andCallFake(function(opt) {
              if (opt.type === "GET") {
                  expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.processData).toEqual(false);
                  opt.success({count : 0});
              } else if (opt.type === "POST") {
                  expect(opt.url).toEqual('/_api/cursor');
                  expect(opt.contentType).toEqual("application/json");
                  expect(opt.cache).toEqual(false);
                  expect(opt.async).toEqual(false);
                  expect(opt.data).toEqual(JSON.stringify({
                      query : "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " +
                          "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x",
                      bindVars : {
                          "@collection" : colid,
                          "offset": 10,
                          "count": 10
                      }
                  }));
                  opt.success({result : [{_id : 1, _rev : 2, _key : 4}, {_id : 2, _rev : 2, _key : 4},
                      {_id : 3, _rev : 2, _key : 4}]
                  });
              }
          });
          spyOn(window.arangoDocumentsStore, "reset");
          spyOn(window.documentsView, "drawTable");
          spyOn(window.documentsView, "renderPagination");
          spyOn(window.documentsView, "initTable");
          var result = col.getDocuments(colid, currpage);
          expect(window.documentsView.initTable).toHaveBeenCalled();
      });

/*
      
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
              error: function(data) {
              }
          });
      }
*/

  });

}());

