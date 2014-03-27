/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
(function () {

    "use strict";

    describe("NotificationCollection", function () {

        var col;

        beforeEach(function () {
            col = new window.NotificationCollection();
        });

        it("NotificationCollection", function () {
            expect(col.model).toEqual(window.Notification);
            expect(col.url).toEqual("");
        });

    })
}());

