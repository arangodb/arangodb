/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The current database", function() {
    
    var model;

    beforeEach(function() {
      model = new window.CurrentDatabase();
    });

    it("should request /_api/database/current on fetch", function() {
      spyOn($, "ajax");
      model.fetch();
      expect($.ajax).toHaveBeenCalledWith("/_api/database/current", {type: "GET"});
    });

  });

}());
