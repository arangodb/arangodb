/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine, window, $, arangoHelper*/

(function () {
  'use strict';

  describe('Arango Collection Model', function () {
    var myCollection;

    beforeEach(function () {
      myCollection = new window.arangoCollectionModel();
    });

    it('verifies urlRoot', function () {
      expect(myCollection.urlRoot).toEqual('/_api/collection');
    });

    it('verifies defaults', function () {
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual('');
      expect(myCollection.get('status')).toEqual('');
      expect(myCollection.get('type')).toEqual('');
      expect(myCollection.get('isSystem')).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });

    it('verifies set defaults', function () {
      var name = 'blub',
        status = 'active',
        type = 'myType';
      myCollection = new window.arangoCollectionModel(
        {
          name: name,
          status: status,
          type: type
        }
      );
      expect(myCollection.get('id')).toEqual('');
      expect(myCollection.get('name')).toEqual(name);
      expect(myCollection.get('status')).toEqual(status);
      expect(myCollection.get('type')).toEqual(type);
      expect(myCollection.isSystem).toBeFalsy();
      expect(myCollection.get('picture')).toEqual('');
    });

    describe('checking ajax', function () {
      var url, type, shouldFail, successResult, failResult, id, data;

      beforeEach(function () {
        id = '4711';
        successResult = 'success';
        failResult = 'fail';
        myCollection.set('id', id);
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual(url);
          expect(opt.type).toEqual(type);
          expect(opt.success).toEqual(jasmine.any(Function));
          expect(opt.error).toEqual(jasmine.any(Function));
          if (opt.data) {
            expect(opt.data).toEqual(data);
          }
          if (shouldFail) {
            opt.error(failResult);
          } else {
            opt.success(successResult);
          }
        });
      });

      describe('getProperties', function () {
        beforeEach(function () {
          url = '/_api/collection/' + id + '/properties';
          type = 'GET';
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.getProperties()).toEqual(successResult);
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.getProperties()).toEqual(failResult);
        });
      });

      describe('getFigures', function () {
        beforeEach(function () {
          url = '/_api/collection/' + id + '/figures';
          type = 'GET';
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.getFigures()).toEqual(successResult);
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.getFigures()).toEqual(failResult);
        });
      });

      describe('getRevision', function () {
        beforeEach(function () {
          url = '/_api/collection/' + id + '/revision';
          type = 'GET';
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.getRevision()).toEqual(successResult);
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.getRevision()).toEqual(failResult);
        });
      });

      describe('getFigures', function () {
        beforeEach(function () {
          url = '/_api/index/?collection=' + id;
          type = 'GET';
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.getIndex()).toEqual(successResult);
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.getIndex()).toEqual(failResult);
        });
      });

      describe('loadCollection', function () {
        beforeEach(function () {
          url = '/_api/collection/' + id + '/load';
          type = 'PUT';
        });

        it('success', function () {
          shouldFail = false;
          spyOn(arangoHelper, 'arangoNotification');
          myCollection.loadCollection();
          expect(myCollection.get('status')).toEqual('loaded');
        });

        it('fail', function () {
          shouldFail = true;
          spyOn(arangoHelper, 'arangoError');
          myCollection.loadCollection();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Collection error');
        });
      });

      describe('unloadCollection', function () {
        beforeEach(function () {
          url = '/_api/collection/' + id + '/unload';
          type = 'PUT';
        });

        it('success', function () {
          shouldFail = false;
          spyOn(arangoHelper, 'arangoNotification');
          myCollection.unloadCollection();
          expect(myCollection.get('status')).toEqual('unloaded');
        });

        it('fail', function () {
          shouldFail = true;
          spyOn(arangoHelper, 'arangoError');
          myCollection.unloadCollection();
          expect(arangoHelper.arangoError).toHaveBeenCalledWith('Collection error');
        });
      });

      describe('renameCollection', function () {
        var name;

        beforeEach(function () {
          name = 'newname';
          url = '/_api/collection/' + id + '/rename';
          type = 'PUT';
          data = JSON.stringify({name: name});
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.renameCollection(name)).toBeTruthy();
          expect(myCollection.get('name')).toEqual(name);
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.renameCollection(name)).toBeFalsy();
          expect(myCollection.get('name')).not.toEqual(name);
        });

        it('fail with parseable error message', function () {
          shouldFail = true;
          failResult = {responseText: '{"errorMessage" : "bla"}'};
          expect(myCollection.renameCollection(name)).toEqual('bla');
          expect(myCollection.get('name')).not.toEqual(name);
        });
      });

      describe('changeCollection', function () {
        var wfs, jns;

        beforeEach(function () {
          wfs = true;
          jns = 10 * 100 * 1000;
          url = '/_api/collection/' + id + '/properties';
          type = 'PUT';
          data = JSON.stringify({waitForSync: wfs, journalSize: jns});
        });

        it('success', function () {
          shouldFail = false;
          expect(myCollection.changeCollection(wfs, jns)).toBeTruthy();
        });

        it('fail', function () {
          shouldFail = true;
          expect(myCollection.changeCollection(wfs, jns)).toBeFalsy();
        });
        it('fail with parseable error message', function () {
          shouldFail = true;
          failResult = {responseText: '{"errorMessage" : "bla"}'};
          expect(myCollection.changeCollection(wfs, jns)).toEqual('bla');
        });
      });
    });
    describe('checking ajax', function () {
      var id;

      beforeEach(function () {
        id = '4711';
        myCollection = new window.arangoCollectionModel({
          id: '4711',
          name: 'name'
        });
      });
      it('should get and index and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index/?collection=' + id);
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.getIndex()).toEqual('success');
      });

      it('should get and index and fail', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index/?collection=' + id);
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.getIndex()).toEqual('error');
      });

      it('should create and index and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index?collection=' + id);
          expect(opt.type).toEqual('POST');
          expect(opt.cache).toEqual(false);
          expect(opt.data).toEqual('{"ab":"CD"}');
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.createIndex({ab: 'CD'})).toEqual(true);
      });

      it('should create and index and fail', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index?collection=' + id);
          expect(opt.type).toEqual('POST');
          expect(opt.cache).toEqual(false);
          expect(opt.data).toEqual('{"ab":"CD"}');
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.createIndex({ab: 'CD'})).toEqual('error');
      });

      it('should delete and index and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index/' + 'name' + '/1');
          expect(opt.type).toEqual('DELETE');
          expect(opt.cache).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.deleteIndex(1)).toEqual(true);
      });

      it('should delete and index and fail', function () {
        var collection = '12345';
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/index/' + 'name' + '/1');
          expect(opt.type).toEqual('DELETE');
          expect(opt.cache).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.deleteIndex(1)).toEqual(false);
      });

      it('should get Properties and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/properties');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.getProperties()).toEqual('success');
      });

      it('should get Properties and fail', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/properties');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.getProperties()).toEqual('error');
      });

      it('should get Figures and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/figures');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.getFigures()).toEqual('success');
      });

      it('should get Figures and fail', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/figures');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.getFigures()).toEqual('error');
      });

      it('should get Revision and succeed', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/revision');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.success('success');
        });
        expect(myCollection.getRevision()).toEqual('success');
      });

      it('should get Revision and fail', function () {
        spyOn($, 'ajax').andCallFake(function (opt) {
          expect(opt.url).toEqual('/_api/collection/' + id + '/revision');
          expect(opt.type).toEqual('GET');
          expect(opt.cache).toEqual(false);
          expect(opt.contentType).toEqual('application/json');
          expect(opt.processData).toEqual(false);
          expect(opt.async).toEqual(false);
          opt.error('error');
        });
        expect(myCollection.getRevision()).toEqual('error');
      });
    });
  });
}());
