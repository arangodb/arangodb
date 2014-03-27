/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
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
    })
}());
