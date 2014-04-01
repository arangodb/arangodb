/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global describe, beforeEach, window, afterEach, it, spyOn, expect,
 require, jasmine,  exports,  */
(function () {

    "use strict";

    describe("StatisticsCollection", function () {

        var col;

        beforeEach(function () {
            col = new window.StatisticsCollection();
        });

        it("StatisticsCollection", function () {
            expect(col.model).toEqual(window.Statistics);
            expect(col.url).toEqual('/_admin/statistics');
        });
    });
}());
