/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
(function () {

    "use strict";

    describe("FoxxCollection", function() {

        var col;

        beforeEach(function() {
            col = new window.FoxxCollection();
        });

        it("FoxxCollection", function() {
            expect(col.model).toEqual(window.Foxx);
            expect(col.url).toEqual("/_admin/aardvark/foxxes");
        });

    })
}());

