/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect, runs, waitsFor*/
/*global _, $, DBSelectionView*/

(function() {
  "use strict";

  describe("DB Selection", function() {
  
    var view,
      list,
      dbCollection,
      fetched,
      current,
      div;

    beforeEach(function() {
      dbCollection = new window.ArangoDatabase();
      list = ["_system", "second", "first"];
      current = new window.CurrentDatabase({
        name: "first",
        isSystem: false
      });
      _.each(list, function(n) {
        dbCollection.add({name: n});
      });
      fetched = false;
      spyOn(dbCollection, "fetch");
      div = document.createElement("div");
      div.id = "dbSelect";
      document.body.appendChild(div);
      view = new DBSelectionView(
        {
          collection: dbCollection,
          current: current
        }
      );
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should display all databases ordered", function() {
      view.render($(div));
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
      view.render($(div));
      expect($(div).find(":selected").attr("id")).toEqual(current.get("name"));
    });

    it("should trigger fetch on collection", function() {
      view.render($(div));
      expect(dbCollection.fetch).toHaveBeenCalled();
    });

    it("should trigger a database switch on click", function() {
      view.render($(div));
      spyOn(dbCollection, "createDatabaseURL").andReturn("switchURL");
      spyOn(location, "replace");
      $("#dbSelectionList").val("second").trigger("change");
      expect(dbCollection.createDatabaseURL).toHaveBeenCalledWith("second");
      expect(location.replace).toHaveBeenCalledWith("switchURL");
    });

    it("should not render the selection if the list has only one element", function() {
      var oneCollection = new window.ArangoDatabase();
      oneCollection.add({name: current});
      spyOn(oneCollection, "fetch");
      view = new DBSelectionView(
        {
          collection: oneCollection,
          current: current
        }
      );
      view.render($(div));
      expect($(div).text().trim()).toEqual("first");
    });
  });
}());
