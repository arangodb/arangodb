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
      div = document.createElement("li");
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
      var dbList = $("#dbs_dropdown", $(div)),
        childs;

      expect(div.childElementCount).toEqual(2);
      childs = $(dbList).children();
      expect(childs.length).toEqual(3);


      expect($("a", $(childs[1])).attr("id")).toEqual(list[0]);
      expect($("a", $(childs[2])).attr("id")).toEqual(list[1]);
    });

    it("should select the current db", function() {
      view.render($(div));
      expect($($(div).children()[0]).text()).toEqual("DB: " + current.get("name") + " ");
    });

    it("should trigger fetch on collection", function() {
      view.render($(div));
      expect(dbCollection.fetch).toHaveBeenCalled();
    });

/*
    it("should trigger a database switch on click", function() {
      view.render($(div));
      spyOn(dbCollection, "createDatabaseURL").andReturn("switchURL");
      spyOn(window.location, "replace");
      $("#dbSelectionList").val("second").trigger("change");
      expect(dbCollection.createDatabaseURL).toHaveBeenCalledWith("second");
      expect(window.location.replace).toHaveBeenCalledWith("switchURL");
    });
*/
    it("should not render the selection if the list has only one element", function() {
      var oneCollection = new window.ArangoDatabase();
      oneCollection.add(current);
      spyOn(oneCollection, "fetch");
      view = new DBSelectionView(
        {
          collection: oneCollection,
          current: current
        }
      );
      view.render($(div));
      expect($(div).text().trim()).toEqual("DB: first");
    });
  });
}());
