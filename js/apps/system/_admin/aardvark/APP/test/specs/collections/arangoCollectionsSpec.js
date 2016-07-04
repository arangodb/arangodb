/* jshint browser: true */
/* jshint unused: false */
/* global describe, arangoHelper, beforeEach, afterEach, it, spyOn, expect*/
/* global $, jasmine*/

(function () {
  'use strict';

  describe('arangoCollections', function () {
    var col;

    beforeEach(function () {
      col = new window.arangoCollections();
      window.collectionsView = new window.CollectionsView({
        collection: col
      });
      window.arangoCollectionsStore = col;
    });

    it('should translate various Status', function () {
      expect(col.translateStatus(0)).toEqual('corrupted');
      expect(col.translateStatus(1)).toEqual('new born collection');
      expect(col.translateStatus(2)).toEqual('unloaded');
      expect(col.translateStatus(3)).toEqual('loaded');
      expect(col.translateStatus(4)).toEqual('in the process of being unloaded');
      expect(col.translateStatus(5)).toEqual('deleted');
      expect(col.translateStatus(6)).toEqual('loading');
      expect(col.translateStatus(7)).toEqual(undefined);
    });

    it('should translate various typePictures', function () {
      expect(col.translateTypePicture('document')).toEqual('img/icon_document.png');
      expect(col.translateTypePicture('edge')).toEqual('img/icon_node.png');
      expect(col.translateTypePicture('unknown')).toEqual('img/icon_unknown.png');
      expect(col.translateTypePicture('default')).toEqual('img/icon_arango.png');
    });

    it('should parse a response', function () {
      var response = {collections: [
          {name: '_aSystemCollection', type: 2, status: 2}
      ]};
      col.parse(response);
      expect(response.collections[0].isSystem).toEqual(true);
      expect(response.collections[0].type).toEqual('document (system)');
      expect(response.collections[0].status).toEqual('unloaded');
      expect(response.collections[0].picture).toEqual('img/icon_arango.png');
    });

    it('should getPosition for loaded documents sorted by type', function () {
      col.models = [
        new window.arangoCollectionModel({isSystem: true, name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({type: 'edge', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({type: 'document', name: 'Heinz Herbert Gustav'}),
        new window.arangoCollectionModel({type: 'document', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({status: 'loaded', name: 'Heinz Herbert Karl'}),
        new window.arangoCollectionModel({status: 'unloaded', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({name: 'Heinbert'})
      ];
      col.searchOptions = {
        searchPhrase: 'Heinz Herbert',
        includeSystem: false,
        includeDocument: true,
        includeEdge: false,
        includeLoaded: true,
        includeUnloaded: false,
        sortBy: 'type',
        sortOrder: 1
      };
      var result = col.getPosition('Heinz Herbert');
      expect(result.prev).not.toBe(null);
      expect(result.next).not.toBe(null);
    });

    it('should getPosition for unloaded edges documents sorted by type', function () {
      col.models = [
        new window.arangoCollectionModel({isSystem: true, name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({type: 'edge', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({type: 'document', name: 'Heinz Herbert Gustav'}),
        new window.arangoCollectionModel({type: 'document', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({status: 'loaded', name: 'Heinz Herbert Karl'}),
        new window.arangoCollectionModel({status: 'unloaded', name: 'Heinz Herbert'}),
        new window.arangoCollectionModel({name: 'Heinbert'})
      ];
      col.searchOptions = {
        searchPhrase: 'Heinz Herbert',
        includeSystem: true,
        includeDocument: false,
        includeEdge: true,
        includeLoaded: false,
        includeUnloaded: true,
        sortBy: 'name',
        sortOrder: 1
      };
      var result = col.getPosition('Heinz Herbert');
      expect(result.prev).not.toBe(null);
      expect(result.next).not.toBe(null);
    });

    it('should create a Collection and succeed', function () {
      var id = '12345', result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/collection');
        expect(opt.type).toEqual('POST');
        expect(opt.cache).toEqual(false);
        expect(opt.contentType).toEqual('application/json');
        expect(opt.processData).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify(
          {
            name: 'heinz', waitForSync: true, journalSize: 2, isSystem: true,
            type: 3, numberOfShards: 3, shardKeys: ['a', 'b', 'c']
          }
        ));
        expect(opt.async).toEqual(false);
        opt.success('success');
      });
      result = col.newCollection('heinz', true, true, 2, 3, 3, ['a', 'b', 'c']);
      expect(result.data).toEqual('success');
      expect(result.status).toEqual(true);
    });

    it('should create a Collection and fail', function () {
      var id = '12345', result;
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.url).toEqual('/_api/collection');
        expect(opt.type).toEqual('POST');
        expect(opt.cache).toEqual(false);
        expect(opt.contentType).toEqual('application/json');
        expect(opt.processData).toEqual(false);
        expect(opt.data).toEqual(JSON.stringify(
          {
            name: 'heinz', waitForSync: true, journalSize: 2, isSystem: true,
            type: 3, numberOfShards: 3, shardKeys: ['a', 'b', 'c']
          }
        ));
        expect(opt.async).toEqual(false);
        opt.error({responseText: '{ "errorMessage" : "went wrong"}'});
      });
      result = col.newCollection('heinz', true, true, 2, 3, 3, ['a', 'b', 'c']);
      expect(result.errorMessage).toEqual('went wrong');
      expect(result.status).toEqual(false);
    });
  });
}());
