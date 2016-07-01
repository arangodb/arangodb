/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, window, $ */

(function () {
  'use strict';

  describe('Arango Document Model', function () {
    var myDocument = new window.arangoDocumentModel();
    it('verifies urlRoot', function () {
      expect(myDocument.urlRoot).toEqual('/_api/document');
    });
  });

  describe('Arango Document Model', function () {
    it('verifies defaults', function () {
      var myDocument = new window.arangoDocumentModel();
      expect(myDocument.get('_id')).toEqual('');
      expect(myDocument.get('_rev')).toEqual('');
      expect(myDocument.get('_key')).toEqual('');
    });
  });

  describe('Arango Document Model', function () {
    it('verifies set defaults', function () {
      var id = 'myID',
        rev = 'myRev',
        key = 'myKey',
        myDocument = new window.arangoDocumentModel(
          {
            _id: id,
            _rev: rev,
            _key: key
          }
      );
      expect(myDocument.get('_id')).toEqual(id);
      expect(myDocument.get('_rev')).toEqual(rev);
      expect(myDocument.get('_key')).toEqual(key);
    });
  });

  describe('Arango Document Model', function () {
    it('verifies getSorted', function () {
      var id = 'myID',
        rev = 'myRev',
        key = 'myKey',
        myDocument = new window.arangoDocumentModel(
          {
            c: 'c',
            _id: id,
            _rev: rev,
            _key: key,
            b: 'b',
            a: 'a'
          }
      );
      expect(myDocument.getSorted()).toEqual(
        {
          _id: id, _key: key, _rev: rev, a: 'a', b: 'b', c: 'c'
        }
      );
    });
  });
}());
