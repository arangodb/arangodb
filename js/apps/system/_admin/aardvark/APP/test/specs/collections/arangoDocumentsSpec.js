/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global $, _*/

(function () {
  'use strict';

  describe('arangoDocuments', function () {
    var col;

    beforeEach(function () {
      window.documentsView = new window.DocumentsView();
      col = new window.arangoDocuments();
      window.arangoDocumentsStore = col;
    });

    afterEach(function () {
      delete window.documentsView;
    });

    describe('navigate', function () {
      beforeEach(function () {
        // This should be 10 pages
        expect(col.pagesize).toEqual(10);
        col.setTotal(100);
        col.setPage(5);
        expect(col.getPage()).toEqual(5);
      });

      it('to first page', function () {
        col.setToFirst();
        expect(col.getPage()).toEqual(1);
      });

      it('to last page', function () {
        col.setToLast();
        expect(col.getPage()).toEqual(10);
      });

      it('to previous page', function () {
        col.setToPrev();
        expect(col.getPage()).toEqual(4);
      });

      it('to next page', function () {
        col.setToNext();
        expect(col.getPage()).toEqual(6);
      });

      it('set page', function () {
        col.setPage(0);
        expect(col.getPage()).toEqual(1);
      });
    });

    describe('getting documents', function () {
      var colId, queryStart, queryEnd, sortStatement, filter1, filter2, ajaxCB;

      beforeEach(function () {
        colId = '12345';
        col.setPage(5);
        ajaxCB = function (obj) {
          expect(_.isFunction(obj.success)).toBeTruthy();
          expect(obj.url).toEqual('/_api/collection/' + colId + '/count');
          expect(obj.type).toEqual('GET');
          expect(obj.async).toBeFalsy();
          obj.success({
            count: 1000
          });
        };
        spyOn($, 'ajax').andCallFake(function (req) {
          ajaxCB(req);
        });
        col.setCollection(colId);
        expect(col.getPage()).toEqual(1);
        expect($.ajax).toHaveBeenCalled();
        queryStart = 'FOR x in @@collection';
        sortStatement = ' SORT TO_NUMBER(x._key) == 0 ? x._key : TO_NUMBER(x._key)';
        filter1 = ' FILTER x.`test` == @param0';
        filter2 = ' && x.`second` < @param1';
        queryEnd = ' LIMIT @offset, @count RETURN x';
        $.ajax.reset();
      });

      it('should start using first page', function () {
        ajaxCB = function (req) {
          expect(req.url).toEqual('/_api/cursor');
          expect(req.type).toEqual('POST');
          expect(req.cache).toEqual(false);
          expect(req.async).toEqual(false);
          var data = JSON.parse(req.data),
            baseQuery = queryStart + sortStatement + queryEnd;
          expect(data.query).toEqual(baseQuery);
          expect(data.bindVars['@collection']).toEqual(colId);
          expect(data.bindVars.offset).toEqual(0);
          expect(data.bindVars.count).toEqual(10);
          expect(req.success).toEqual(jasmine.any(Function));
        };
        col.getDocuments();
        expect($.ajax).toHaveBeenCalled();
      });

      it('should react to page changes', function () {
        col.setPage(3);
        ajaxCB = function (req) {
          expect(req.url).toEqual('/_api/cursor');
          expect(req.type).toEqual('POST');
          expect(req.cache).toEqual(false);
          expect(req.async).toEqual(false);
          var data = JSON.parse(req.data),
            baseQuery = queryStart + sortStatement + queryEnd;
          expect(data.query).toEqual(baseQuery);
          expect(data.bindVars['@collection']).toEqual(colId);
          expect(data.bindVars.offset).toEqual(20);
          expect(data.bindVars.count).toEqual(10);
          expect(req.success).toEqual(jasmine.any(Function));
        };
        col.getDocuments();
        expect($.ajax).toHaveBeenCalled();
      });

      it('should not sort large collections', function () {
        col.setTotal(10000);
        ajaxCB = function (req) {
          expect(req.url).toEqual('/_api/cursor');
          expect(req.type).toEqual('POST');
          expect(req.cache).toEqual(false);
          expect(req.async).toEqual(false);
          var data = JSON.parse(req.data),
            baseQuery = queryStart + queryEnd;
          expect(data.query).toEqual(baseQuery);
          expect(data.bindVars['@collection']).toEqual(colId);
          expect(data.bindVars.offset).toEqual(0);
          expect(data.bindVars.count).toEqual(10);
          expect(req.success).toEqual(jasmine.any(Function));
        };
        col.getDocuments();
        expect($.ajax).toHaveBeenCalled();
      });

      it('should be able to use one filter', function () {
        col.addFilter('test', '==', 'foxx');
        ajaxCB = function (req) {
          expect(req.url).toEqual('/_api/cursor');
          expect(req.type).toEqual('POST');
          expect(req.cache).toEqual(false);
          expect(req.async).toEqual(false);
          var data = JSON.parse(req.data),
            baseQuery = queryStart + filter1 + sortStatement + queryEnd;
          expect(data.query).toEqual(baseQuery);
          expect(data.bindVars['@collection']).toEqual(colId);
          expect(data.bindVars.offset).toEqual(0);
          expect(data.bindVars.count).toEqual(10);
          expect(data.bindVars.param0).toEqual('foxx');
          expect(req.success).toEqual(jasmine.any(Function));
        };
        col.getDocuments();
        expect($.ajax).toHaveBeenCalled();
      });

      it('should be able to use a second filter', function () {
        col.addFilter('test', '==', 'other');
        col.addFilter('second', '<', 'params');
        ajaxCB = function (req) {
          expect(req.url).toEqual('/_api/cursor');
          expect(req.type).toEqual('POST');
          expect(req.cache).toEqual(false);
          expect(req.async).toEqual(false);
          var data = JSON.parse(req.data),
            baseQuery = queryStart + filter1 + filter2 + sortStatement + queryEnd;
          expect(data.query).toEqual(baseQuery);
          expect(data.bindVars['@collection']).toEqual(colId);
          expect(data.bindVars.offset).toEqual(0);
          expect(data.bindVars.count).toEqual(10);
          expect(data.bindVars.param0).toEqual('other');
          expect(data.bindVars.param1).toEqual('params');
          expect(req.success).toEqual(jasmine.any(Function));
        };
        col.getDocuments();
        expect($.ajax).toHaveBeenCalled();
      });

      it('should insert the result of the query appropriatly', function () {
        var f = {_id: '1/1', _rev: 2, _key: 1},
          s = {_id: '1/2', _rev: 2, _key: 2},
          t = {_id: '1/3', _rev: 2, _key: 3};
        ajaxCB = function (req) {
          req.success({result: [f, s, t], extra: {fullCount: 3}});
        };
        expect(col.getTotal()).not.toEqual(3);
        col.getDocuments();
        expect(col.getTotal()).toEqual(3);
        expect(col.size()).toEqual(3);
        expect(col.findWhere({content: f})).toBeDefined();
        expect(col.findWhere({content: s})).toBeDefined();
        expect(col.findWhere({content: t})).toBeDefined();
      });
    });

    it('start succesful Upload mit XHR ready state = 4, ' +
      'XHR status = 201 and parseable JSON', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/import?type=auto&collection=' +
            encodeURIComponent(col.collectionID) +
            '&createCollection=false');
          expect(opt.dataType).toEqual('json');
          expect(opt.contentType).toEqual('json');
          expect(opt.processData).toEqual(false);
          expect(opt.data).toEqual('a');
          expect(opt.async).toEqual(false);
          expect(opt.type).toEqual('POST');
          opt.complete({readyState: 4, status: 201, responseText: '{"a" : 1}'});
        });
        var res = col.updloadDocuments('a');
        expect(res).toEqual(true);
      });

    it('start succesful Upload mit XHR ready state != 4', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/import?type=auto&collection=' +
          encodeURIComponent(col.collectionID) +
          '&createCollection=false');
        expect(opt.dataType).toEqual('json');
        expect(opt.contentType).toEqual('json');
        expect(opt.processData).toEqual(false);
        expect(opt.data).toEqual('a');
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('POST');
        opt.complete({readyState: 3, status: 201, responseText: '{"a" : 1}'});
      });

      var res = col.updloadDocuments('a');
      expect(res).toEqual('Upload error');
    });
  });
}());
