/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jQuery*/
/* global runs, waitsFor, jasmine, waits*/
/* global $, console, arangoHelper */
(function () {
  'use strict';
  describe('CollectionsItem View', function () {
    var myStore,
      isCoordinator,
      myView,
      div,
      modalDiv,
      edgeCol,
      docCol,
      sysCol,
      tile1,
      tile2,
      tile3;

    beforeEach(function () {
      isCoordinator = false;

      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);

      modalDiv = document.createElement('div');
      modalDiv.id = 'modalPlaceholder';
      document.body.appendChild(modalDiv);

      edgeCol = new window.arangoCollectionModel({
        id: 'e',
        type: 'edge',
        isSystem: false,
        name: 'e',
        status: 'unloaded',
        journalSize: 33554432
      });
      docCol = new window.arangoCollectionModel({
        id: 'd',
        type: 'document',
        isSystem: false,
        name: 'd',
        status: 'loaded',
        journalSize: 33554432
      });
      sysCol = new window.arangoCollectionModel({
        id: 's',
        type: 'document',
        isSystem: true,
        name: '_sys',
        status: 'unloaded',
        journalSize: 33554432
      });

      var cols = [edgeCol, docCol, sysCol];
      spyOn($, 'ajax').andCallFake(function (url) {
        return {done: function () {}};
      });
      myStore = new window.arangoCollections(cols);
      spyOn(window, 'isCoordinator').andReturn(isCoordinator);
      myView = new window.CollectionsView({
        collection: myStore
      });
      myView.render();

      // render tiles
      tile1 = new window.CollectionListItemView({
        model: edgeCol,
        collectionsView: myView
      });
      $('#collectionsThumbnailsIn').append(tile1.render().el);

      tile2 = new window.CollectionListItemView({
        model: docCol,
        collectionsView: myView
      });
      $('#collectionsThumbnailsIn').append(tile2.render().el);

      tile3 = new window.CollectionListItemView({
        model: sysCol,
        collectionsView: myView
      });
      $('#collectionsThumbnailsIn').append(tile3.render().el);

      window.modalView = new window.ModalView();
    });

    afterEach(function () {
      delete window.modalView;
      document.body.removeChild(div);
      document.body.removeChild(modalDiv);
    });

    describe('test', function () {
      it('should draw a property modal for loaded collection', function () {
        spyOn(tile2.model, 'getProperties').andCallFake(function () {
          return {
            journalSize: 33554432,
            waitForSync: true
          };
        });

        spyOn(window.modalView, 'show');

        var e = jQuery.Event();
        tile2.editProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should draw a property modal for unloaded collection', function () {
        spyOn(tile1.model, 'getProperties').andCallFake(function () {
          return {
            journalSize: 33554432,
            waitForSync: true
          };
        });

        spyOn(window.modalView, 'show');

        var e = jQuery.Event();
        tile1.editProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should show a info modal for loaded collection (unloaded has no info modal)', function () {
        spyOn(window.modalView, 'show');

        var e = jQuery.Event();
        tile2.showProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it('should stop an events propagination', function () {
        var e = {
          stopPropagation: function () {}
        };
        spyOn(e, 'stopPropagation');
        tile1.noop(e);
        expect(e.stopPropagation).toHaveBeenCalled();
      });

      it('should load a collection', function () {
        spyOn(tile1.model, 'loadCollection');
        spyOn(tile1, 'render');
        spyOn(window.modalView, 'hide');
        tile1.loadCollection();
        expect(tile1.model.loadCollection).toHaveBeenCalled();
        expect(tile1.render).toHaveBeenCalled();
        expect(window.modalView.hide).toHaveBeenCalled();
      });

      it('should unload a collection', function () {
        spyOn(tile1.model, 'unloadCollection');
        spyOn(tile1, 'render');
        spyOn(window.modalView, 'hide');
        tile1.unloadCollection();
        expect(tile1.model.unloadCollection).toHaveBeenCalled();
        expect(tile1.render).toHaveBeenCalled();
        expect(window.modalView.hide).toHaveBeenCalled();
      });

      /* TODO Fix
      it("should delete a collection with success", function() {
        spyOn(tile1.model, "destroy")
        spyOn(tile1.collectionsView, "render")
        spyOn(window.modalView, "hide")
        tile1.deleteCollection()
        expect(tile1.model.destroy).toHaveBeenCalled()
        expect(tile1.collectionsView.render).toHaveBeenCalled()
        expect(window.modalView.hide).toHaveBeenCalled()
      })
      */

      describe('saving a modified collection', function () {
        beforeEach(function () {
          window.App = {
            notificationList: {
              add: function () {
                return undefined;
              }
            }
          };
        });

        afterEach(function () {
          delete window.App;
        });

        it('unloaded collection, save error', function () {
          spyOn(arangoHelper, 'arangoError');
          spyOn(tile1.model, 'renameCollection');

          tile1.saveModifiedCollection();

          expect(tile1.model.renameCollection).toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalled();
        });

        it('should save a modified collection (loaded collection, save error)', function () {
          tile1.model.set('status', 'loaded');
          spyOn(arangoHelper, 'arangoError');
          spyOn(tile1.model, 'renameCollection');

          tile1.saveModifiedCollection();

          expect(tile1.model.renameCollection).toHaveBeenCalled();
          expect(arangoHelper.arangoError).toHaveBeenCalled();
        });
      });

      it('should save a modified collection (unloaded collection, success)', function () {
        spyOn(tile1.model, 'renameCollection').andReturn(true);
        spyOn(tile1.collectionsView, 'render');
        spyOn(window.modalView, 'hide');
        tile1.saveModifiedCollection();

        expect(window.modalView.hide).toHaveBeenCalled();
        expect(tile1.collectionsView.render).toHaveBeenCalled();
        expect(tile1.model.renameCollection).toHaveBeenCalled();
      });

      it('should save a modified collection (loaded collection, success)', function () {
        tile1.model.set('status', 'loaded');
        var tempdiv = document.createElement('div');
        tempdiv.id = 'change-collection-size';
        document.body.appendChild(tempdiv);
        $('#change-collection-size').val(123123123123);

        spyOn(tile1.model, 'changeCollection').andReturn(true);
        spyOn(tile1.model, 'renameCollection').andReturn(true);
        spyOn(tile1.collectionsView, 'render');
        spyOn(window.modalView, 'hide');
        tile1.saveModifiedCollection();

        expect(window.modalView.hide).toHaveBeenCalled();
        expect(tile1.collectionsView.render).toHaveBeenCalled();
        expect(tile1.model.renameCollection).toHaveBeenCalled();

        document.body.removeChild(tempdiv);
      });

      it('should not save a modified collection (invalid data, result)', function () {
        tile1.model.set('status', 'loaded');
        var tempdiv = document.createElement('div');
        tempdiv.id = 'change-collection-size';
        document.body.appendChild(tempdiv);
        $('#change-collection-size').val(123123123123);

        spyOn(arangoHelper, 'arangoError');
        spyOn(arangoHelper, 'arangoNotification');
        spyOn(tile1.model, 'changeCollection').andReturn(false);
        spyOn(tile1.model, 'renameCollection').andReturn(false);
        spyOn(tile1.collectionsView, 'render');
        spyOn(window.modalView, 'hide');
        tile1.saveModifiedCollection();

        expect(window.modalView.hide).not.toHaveBeenCalled();
        expect(tile1.collectionsView.render).not.toHaveBeenCalled();
        expect(tile1.model.renameCollection).toHaveBeenCalled();
        expect(arangoHelper.arangoError).toHaveBeenCalled();

        document.body.removeChild(tempdiv);
      });

      it('should not save a modified collection (invalid data, result)', function () {
        tile1.model.set('status', 'loaded');
        var tempdiv = document.createElement('div');
        tempdiv.id = 'change-collection-size';
        document.body.appendChild(tempdiv);
        $('#change-collection-size').val(123123123123);

        spyOn(arangoHelper, 'arangoError');
        spyOn(arangoHelper, 'arangoNotification');
        spyOn(tile1.model, 'changeCollection').andReturn(false);
        spyOn(tile1.model, 'renameCollection').andReturn(true);
        spyOn(tile1.collectionsView, 'render');
        spyOn(window.modalView, 'hide');
        tile1.saveModifiedCollection();

        expect(window.modalView.hide).not.toHaveBeenCalled();
        expect(tile1.collectionsView.render).not.toHaveBeenCalled();
        expect(tile1.model.renameCollection).toHaveBeenCalled();
        expect(arangoHelper.arangoNotification).toHaveBeenCalled();

        document.body.removeChild(tempdiv);
      });
    });
  });
}());
