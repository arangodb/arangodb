/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/*global $, _*/

(function () {
    "use strict";

    describe("arangoLogs", function () {

        var col;

        beforeEach(function () {
            col = new window.ArangoLogs({upto : true, loglevel : 1});
        });

        describe("navigate", function() {

          beforeEach(function() {
            // This should be 10 pages
            expect(col.pagesize).toEqual(10);
            col.setTotal(100);
            col.setPage(5);
            expect(col.getPage()).toEqual(5);
          });

          it("to first page", function () {
            col.setToFirst();
            expect(col.getPage()).toEqual(1);
          });

          it("to last page", function () {
            col.setToLast();
            expect(col.getPage()).toEqual(10);
          });

          it("to previous page", function () {
            col.setToPrev();
            expect(col.getPage()).toEqual(4);
          });

          it("to next page", function () {
            col.setToNext();
            expect(col.getPage()).toEqual(6);
          });

          it("set page", function () {
             col.setPage(0);
             expect(col.getPage()).toEqual(1);
          });

        });

        it("parse", function() {

            expect(col.parse(
                {
                    lid : [1],
                    level : [1],
                    text: ["bla"],
                    timestamp : ["123"],
                    totalAmount : 1
                }
            )).toEqual([
                    {
                        lid : 1,
                        level : 1,
                        text: "bla",
                        timestamp : "123",
                        totalAmount : 1
                    }
                ]
            );
            expect(col.totalAmount).toEqual(1);
        });

        it("initialize", function() {
            col.initialize({upto : true, loglevel : 1});
            expect(col.upto).toEqual(true);
            expect(col.loglevel).toEqual(1);
        });


        it("url with upto", function() {
            col.initialize({upto : true, loglevel : 1});
            col.page = 1;
            col.pagesize = 2;

            expect(col.url()).toEqual('/_admin/log?upto=1&size=2&offset=2');
        });

        it("url with no upto", function() {
            col.initialize({upto : false, loglevel : 1});
            col.upto = false;
            col.page = 1;
            col.pagesize = 2;

            expect(col.url()).toEqual('/_admin/log?level=1&size=2&offset=2');
        });
    });


}());


