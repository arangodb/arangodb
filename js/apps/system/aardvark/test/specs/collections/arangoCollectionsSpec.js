/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("arangoCollections", function() {
    
    var col;

    beforeEach(function() {
        col = new window.arangoCollections();
    });

    it("should translate various Status", function() {
        expect(col.translateStatus(0)).toEqual("corrupted");
        expect(col.translateStatus(1)).toEqual("new born collection");
        expect(col.translateStatus(2)).toEqual("unloaded");
        expect(col.translateStatus(3)).toEqual("loaded");
        expect(col.translateStatus(4)).toEqual("in the process of being unloaded");
        expect(col.translateStatus(5)).toEqual("deleted");
        expect(col.translateStatus(6)).toEqual("loading");
        expect(col.translateStatus(7)).toEqual(undefined);
    });

    it("should translate various typePictures", function() {
      expect(col.translateTypePicture('document')).toEqual("img/icon_document.png");
      expect(col.translateTypePicture('edge')).toEqual("img/icon_node.png");
      expect(col.translateTypePicture('unknown')).toEqual("img/icon_unknown.png");
      expect(col.translateTypePicture("default")).toEqual("img/icon_arango.png");
    });

    it("should parse a response", function() {
      var response = {collections: [{name : "_aSystemCollection", type: 2, status: 2}]};
      col.parse(response);
      expect(response.collections[0].isSystem).toEqual(true);
      expect(response.collections[0].type).toEqual("document (system)");
      expect(response.collections[0].status).toEqual("unloaded");
      expect(response.collections[0].picture).toEqual("img/icon_arango.png");
    });

    it("should getPosition for loaded documents sorted by type", function() {

      col.models = [
        new window.arangoCollection({isSystem : true, name: "Heinz Herbert"}),
        new window.arangoCollection({type : 'edge', name: "Heinz Herbert"}),
        new window.arangoCollection({type: 'document', name: "Heinz Herbert Gustav"}),
        new window.arangoCollection({type: 'document', name: "Heinz Herbert"}),
        new window.arangoCollection({status: 'loaded', name: "Heinz Herbert Karl"}),
        new window.arangoCollection({status: 'unloaded', name: "Heinz Herbert"}),
        new window.arangoCollection({name: "Heinbert"})
      ];
      col.searchOptions = {
          searchPhrase: "Heinz Herbert",
          includeSystem: false,
          includeDocument: true,
          includeEdge: false,
          includeLoaded: true,
          includeUnloaded: false,
          sortBy: 'type',
          sortOrder: 1
      }
      var result = col.getPosition("Heinz Herbert");
      expect(result.prev).not.toBe(null);
      expect(result.next).not.toBe(null);
    });

    it("should getPosition for unloaded edges documents sorted by type", function() {

      col.models = [
          new window.arangoCollection({isSystem : true, name: "Heinz Herbert"}),
          new window.arangoCollection({type : 'edge', name: "Heinz Herbert"}),
          new window.arangoCollection({type: 'document', name: "Heinz Herbert Gustav"}),
          new window.arangoCollection({type: 'document', name: "Heinz Herbert"}),
          new window.arangoCollection({status: 'loaded', name: "Heinz Herbert Karl"}),
          new window.arangoCollection({status: 'unloaded', name: "Heinz Herbert"}),
          new window.arangoCollection({name: "Heinbert"})
      ];
      col.searchOptions = {
          searchPhrase: "Heinz Herbert",
          includeSystem: true,
          includeDocument: false,
          includeEdge: true,
          includeLoaded: false,
          includeUnloaded: true,
          sortBy: 'name',
          sortOrder: 1
      }
      var result = col.getPosition("Heinz Herbert");
      expect(result.prev).not.toBe(null);
      expect(result.next).not.toBe(null);
    });
  });

}());

