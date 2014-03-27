/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("Arango Statistics Model", function() {
  it("verifies url", function() {
    var myStatistics = new window.Statistics();
    expect(myStatistics.url())
      .toEqual("/_admin/statistics");
  });
});

}());