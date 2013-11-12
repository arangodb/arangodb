/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global _, $, DBSelectionView*/

(function() {
  "use strict";

  describe("DB Selection", function() {
  
    var view,
      list,
      dbCollection,
      current,
      div;

    beforeEach(function() {
      dbCollection = new window.ArangoDatabase();
      list = ["_system", "second", "first"];
      current = "first";
      _.each(list, function(n) {
        dbCollection.add({name: n});
      });
      spyOn(dbCollection, "fetch");
      spyOn(dbCollection, "getCurrentDatabase").andCallFake(
        function() {
          return current;
        }
      );
      div = document.createElement("div");
      div.id = "selectDB";
      document.body.appendChild(div);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should display all databases ordered", function() {
      view = new DBSelectionView({collection: dbCollection});
      view.render($("#selectDB"));
      var select = $(div).children()[0],
        childs;
      expect(div.childElementCount).toEqual(1);
      childs = $(select).children();
      expect(childs.length).toEqual(3);
      expect(childs[0].id).toEqual(list[0]);
      expect(childs[1].id).toEqual(list[2]);
      expect(childs[2].id).toEqual(list[1]);
    });

    it("should select the current db", function() {
      view = new DBSelectionView({collection: dbCollection});
      view.render($("#selectDB"));
      expect($(div).find(":selected").attr("id")).toEqual(current);
    });

    it("should trigger fetch on collection", function() {
      view = new DBSelectionView({collection: dbCollection});
      expect(dbCollection.fetch).toHaveBeenCalled();
    });

    it("should trigger a database switch on click", function() {
      view = new DBSelectionView({collection: dbCollection});
      view.render($("#selectDB"));
      spyOn(dbCollection, "createDatabaseURL").andReturn("switchURL");
      spyOn(location, "replace");
      $("#dbSelectionList").val("second").trigger("change");
      expect(dbCollection.createDatabaseURL).toHaveBeenCalledWith("second");
      expect(location.replace).toHaveBeenCalledWith("switchURL");
    });

    it("should not render if the list has only one element", function() {
      var oneCollection = new window.ArangoDatabase();
      oneCollection.add({name: current});
      spyOn(oneCollection, "getCurrentDatabase").andCallFake(
        function() {
          return current;
        }
      );
      view = new DBSelectionView({collection: oneCollection});
      view.render($("#selectDB"));
      expect($(div).text().trim()).toEqual("");
    });
  });
}());
