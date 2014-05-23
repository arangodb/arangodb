/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine, waits*/
/*global $, console, arangoHelper */
(function () {
  "use strict";
  describe("CollectionsItem View", function () {
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

      div = document.createElement("div");
      div.id = "content";
      document.body.appendChild(div);

      modalDiv = document.createElement("div");
      modalDiv.id = "modalPlaceholder";
      document.body.appendChild(modalDiv);

      edgeCol = new window.arangoCollectionModel({
        id: "e",
        type: "edge",
        isSystem: false,
        name: "e",
        status: "unloaded",
        journalSize: 33554432
      });
      docCol = new window.arangoCollectionModel({
        id: "d",
        type: "document",
        isSystem: false,
        name: "d",
        status: "loaded",
        journalSize: 33554432
      });
      sysCol = new window.arangoCollectionModel({
        id: "s",
        type: "document",
        isSystem: true,
        name: "_sys",
        status: "unloaded",
        journalSize: 33554432
      });


      var cols = [edgeCol, docCol, sysCol];
      spyOn($, "ajax").andCallFake(function (url) {
        return {done: function () {
        }};
      });
      myStore = new window.arangoCollections(cols);
      spyOn(window, "isCoordinator").andReturn(isCoordinator);
      myView = new window.CollectionsView({
        collection: myStore
      });
      myView.render();

      //render tiles
      tile1 = new window.CollectionListItemView({
        model: edgeCol,
        collectionsView: myView
      })
      $('#collectionsThumbnailsIn').append(tile1.render().el);

      tile2 = new window.CollectionListItemView({
        model: docCol,
        collectionsView: myView
      })
      $('#collectionsThumbnailsIn').append(tile2.render().el);

      tile3 = new window.CollectionListItemView({
        model: sysCol,
        collectionsView: myView
      })
      $('#collectionsThumbnailsIn').append(tile3.render().el);

      window.modalView = new window.ModalView();
    });

    afterEach(function () {
      document.body.removeChild(div);
      document.body.removeChild(modalDiv);
    });

    describe("test", function () {

      it("should draw a property modal for loaded collection", function () {
        spyOn(tile2.model, "getProperties").andCallFake(function(){
          return {
            journalSize: 33554432,
            waitForSync: true
          }
        });

        spyOn(window.modalView, "show");

        var e = jQuery.Event();
        tile2.editProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it("should draw a property modal for unloaded collection", function () {
        spyOn(tile1.model, "getProperties").andCallFake(function() {
          return {
            journalSize: 33554432,
            waitForSync: true
          }
        });

        spyOn(window.modalView, "show");

        var e = jQuery.Event();
        tile1.editProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

      it("should show a info modal for loaded collection (unloaded has no info modal)", function() {
        spyOn(window.modalView, "show");

        var e = jQuery.Event();
        tile2.showProperties(e);
        expect(window.modalView.show).toHaveBeenCalled();
      });

    });

  });
}());
