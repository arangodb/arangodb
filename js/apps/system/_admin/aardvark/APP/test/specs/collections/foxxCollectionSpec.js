/*jshint browser: true */
/*jshint unused: false */
/*global require, describe, beforeEach, it, expect, exports, Backbone, window, $, arangoLog */
(function () {

    "use strict";

    describe("FoxxCollection", function () {

        var col;

        beforeEach(function () {
            col = new window.FoxxCollection();
        });

        it("FoxxCollection", function () {
            expect(col.model).toEqual(window.Foxx);
            expect(col.url).toEqual("/_admin/aardvark/foxxes");
        });

    });
}());

