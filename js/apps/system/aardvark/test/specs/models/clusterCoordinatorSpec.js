/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("ClusterCoordinator", function() {
    var clusterCoordinator = new window.ClusterCoordinator();
    it("forList", function() {
      expect(clusterCoordinator.forList()).toEqual({
          name: clusterCoordinator.get("name"),
          status: clusterCoordinator.get("status"),
          url: clusterCoordinator.get("url")
      });
    });
  });

}());