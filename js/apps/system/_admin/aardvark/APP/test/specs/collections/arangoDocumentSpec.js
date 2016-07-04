/* jshint browser: true */
/* jshint unused: false */
/* global XMLHttpRequestException, describe, beforeEach, afterEach, it, spyOn,
 arangoHelper, expect*/
/* global $*/

(function () {
  'use strict';

  describe('arangoDocument', function () {
    var col;

    beforeEach(function () {
      col = new window.arangoDocument();
      window.arangoDocumentStore = col;
    });

    it('should deleteEdge and succeed', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('DELETE');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success();
      });
      result = col.deleteEdge(colid, docid);
      expect(result).toEqual(true);
    });

    it('should deleteEdge and fail', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('DELETE');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error();
      });
      result = col.deleteEdge(colid, docid);
      expect(result).toEqual(false);
    });
    it('should deleteEdge and throw exception', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andThrow(XMLHttpRequestException);
      result = col.deleteEdge(colid, docid);
      expect(result).toEqual(false);
    });

    it('should deleteDocument and succeed', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('DELETE');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success();
      });
      result = col.deleteDocument(colid, docid);
      expect(result).toEqual(true);
    });

    it('should deleteDocument and fail', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('DELETE');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error();
      });
      result = col.deleteDocument(colid, docid);
      expect(result).toEqual(false);
    });
    it('should deleteDocument and throw exception', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andThrow(XMLHttpRequestException);
      result = col.deleteDocument(colid, docid);
      expect(result).toEqual(false);
    });

    it('should createTypeEdge and succeed', function () {
      var collectionID = 1, from = 2, to = 3, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual(
          '/_api/edge?collection=' + collectionID + '&from=' + from + '&to=' + to);
        expect(opt.type).toEqual('POST');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({}));
        expect(opt.async).toEqual(false);
        opt.success({_id: 100});
      });
      result = col.createTypeEdge(collectionID, from, to);
      expect(result).toEqual(100);
    });

    it('should createTypeEdge and fail', function () {
      var collectionID = 1, from = 2, to = 3, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual(
          '/_api/edge?collection=' + collectionID + '&from=' + from + '&to=' + to);
        expect(opt.type).toEqual('POST');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({}));
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.createTypeEdge(collectionID, from, to);
      expect(result).toEqual(false);
    });

    it('should createTypeDocument and succeed', function () {
      var collectionID = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document?collection=' + collectionID);
        expect(opt.type).toEqual('POST');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({}));
        expect(opt.async).toEqual(false);
        opt.success({_id: 100});
      });
      result = col.createTypeDocument(collectionID);
      expect(result).toEqual(100);
    });

    it('should createTypeDocument and fail', function () {
      var collectionID = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document?collection=' + collectionID);
        expect(opt.type).toEqual('POST');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({}));
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.createTypeDocument(collectionID);
      expect(result).toEqual(false);
    });

    it('should addDocument and succeed', function () {
      var collectionID = 1, result;
      spyOn(col, 'createTypeDocument');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document?collection=' + collectionID);
        expect(opt.type).toEqual('POST');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify({}));
        expect(opt.async).toEqual(false);
        opt.success({_id: 100});
      });
      result = col.addDocument(collectionID);
      expect(col.createTypeDocument).toHaveBeenCalled();
    });

    it('should getCollectionInfo and succeed', function () {
      var identifier = 1, result;
      spyOn(arangoHelper, 'getRandomToken').andReturn('2');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/collection/' + identifier + '?' + '2');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success({_id: 100});
      });
      result = col.getCollectionInfo(identifier);
      expect(result).toEqual({_id: 100});
    });

    it('should getCollectionInfo and fail', function () {
      var identifier = 1, result;
      spyOn(arangoHelper, 'getRandomToken').andReturn('2');
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/collection/' + identifier + '?' + '2');
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.getCollectionInfo(identifier);
      expect(result).toEqual({});
    });

    it('should getEdge and succeed', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success('bla');
      });
      spyOn(window.arangoDocumentStore, 'add');
      result = col.getEdge(colid, docid);
      expect(result).toEqual(true);
      expect(window.arangoDocumentStore.add).toHaveBeenCalledWith('bla');
    });

    it('should getEdge and fail', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.getEdge(colid, docid);
      expect(result).toEqual(false);
    });

    it('should getDocument and succeed', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success('bla');
      });
      spyOn(window.arangoDocumentStore, 'add');
      result = col.getDocument(colid, docid);
      expect(result).toEqual(true);
      expect(window.arangoDocumentStore.add).toHaveBeenCalledWith('bla');
    });

    it('should getDocument and fail', function () {
      var docid = 1, colid = 1, result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('GET');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.getDocument(colid, docid);
      expect(result).toEqual(false);
    });

    it('should saveEdge and succeed', function () {
      var docid = 1, colid = 1, result, model = {name: 'heinz'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(model);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success('bla');
      });
      result = col.saveEdge(colid, docid, model);
      expect(result).toEqual(true);
    });

    it('should saveEdge and fail', function () {
      var docid = 1, colid = 1, result, model = {name: 'heinz'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/edge/' + colid + '/' + docid);
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(model);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.saveEdge(colid, docid, model);
      expect(result).toEqual(false);
    });

    it('should saveDocument and succeed', function () {
      var docid = 1, colid = 1, result, model = {name: 'heinz'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(model);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.success('bla');
      });
      result = col.saveDocument(colid, docid, model);
      expect(result).toEqual(true);
    });

    it('should saveDocument and fail', function () {
      var docid = 1, colid = 1, result, model = {name: 'heinz'};
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/document/' + colid + '/' + docid);
        expect(opt.type).toEqual('PUT');
        expect(opt.contentType).toEqual('application/json');
        expect(opt.cache).toEqual(false);
        expect(opt.data).toEqual(model);
        expect(opt.processData).toEqual(false);
        expect(opt.async).toEqual(false);
        opt.error({});
      });
      result = col.saveDocument(colid, docid, model);
      expect(result).toEqual(false);
    });

    it('should updateLocalDocument', function () {
      spyOn(window.arangoDocumentStore, 'reset');
      var result = col.updateLocalDocument({x: 'y'});
      expect(window.arangoDocumentStore.reset).toHaveBeenCalled();
    });
    it('clearDocument', function () {
      spyOn(window.arangoDocumentStore, 'reset');
      col.clearDocument();
      expect(window.arangoDocumentStore.reset).toHaveBeenCalled();
    });
  });
}());
