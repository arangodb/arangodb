/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, window, $, arangoLog */
(function () {

    "use strict";

    describe("GraphCollection", function () {

        var col;

        beforeEach(function () {
            col = new window.GraphCollection();
        });

        it("parse", function () {
            expect(col.model).toEqual(window.Graph);
            expect(col.comparator).toEqual("_key");
            expect(col.url).toEqual("/_api/graph");
            expect(col.parse({error: false, graphs: "blub"})).toEqual("blub");
            expect(col.parse({error: true, graphs: "blub"})).toEqual(undefined);
        });

    });
}());
