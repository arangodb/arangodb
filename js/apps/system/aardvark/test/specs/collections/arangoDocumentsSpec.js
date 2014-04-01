/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function () {
    "use strict";

    describe("arangoDocuments", function () {

        var col;

        beforeEach(function () {
            window.documentsView = new window.DocumentsView();
            col = new window.arangoDocuments();
            window.arangoDocumentsStore = col;
        });

        it("should getFirstDocuments", function () {
            expect(col.currentPage).toEqual(1);
            expect(col.collectionID).toEqual(1);
            expect(col.totalPages).toEqual(1);
            expect(col.documentsPerPage).toEqual(10);
            expect(col.documentsCount).toEqual(1);
            expect(col.offset).toEqual(0);
            col.currentPage = 2;
            window.location.hash = "a/b/c";
            col.getFirstDocuments();
            expect(window.location.hash).toEqual("#a/b/c/1");
        });

        it("should getLastDocuments", function () {
            col.currentPage = 2;
            col.totalPages = 5;
            window.location.hash = "a/b/c";
            col.getLastDocuments();
            expect(window.location.hash).toEqual("#a/b/c/5");
        });
        it("should getPrevDocuments", function () {
            col.currentPage = 2;
            window.location.hash = "a/b/c";
            col.getPrevDocuments();
            expect(window.location.hash).toEqual("#a/b/c/1");
        });

        it("should getNextDocuments", function () {
            col.currentPage = 2;
            col.totalPages = 5;
            window.location.hash = "a/b/c";
            col.getNextDocuments();
            expect(window.location.hash).toEqual("#a/b/c/3");
        });

        it("should getDocuments starting on first page and succeed", function () {
            var colid = "12345", currpage = "0", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                if (opt.type === "GET") {
                    expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.processData).toEqual(false);
                    opt.success({count: 100});
                } else if (opt.type === "POST") {
                    expect(opt.url).toEqual('/_api/cursor');
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.data).toEqual(JSON.stringify({
                        query: "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " +
                            "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x",
                        bindVars: {
                            "@collection": colid,
                            "offset": 0,
                            "count": 10
                        }
                    }));
                    opt.success({result: [
                        {_id: 1, _rev: 2, _key: 4},
                        {_id: 2, _rev: 2, _key: 4},
                        {_id: 3, _rev: 2, _key: 4}
                    ]
                    });
                }
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            result = col.getDocuments(colid, currpage);
            expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10);
        });

        it("should getDocuments starting on undefined page and succeed", function () {
            var colid = "12345", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                if (opt.type === "GET") {
                    expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.processData).toEqual(false);
                    opt.success({count: 100});
                } else if (opt.type === "POST") {
                    expect(opt.url).toEqual('/_api/cursor');
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.data).toEqual(JSON.stringify({
                        query: "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " +
                            "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x",
                        bindVars: {
                            "@collection": colid,
                            "offset": 0,
                            "count": 10
                        }
                    }));
                    opt.success({result: [
                        {_id: 1, _rev: 2, _key: 4},
                        {_id: 2, _rev: 2, _key: 4},
                        {_id: 3, _rev: 2, _key: 4}
                    ]
                    });
                }
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            result = col.getDocuments(colid);
            expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10);
        });

        it("should getDocuments with exceeding sort count", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                if (opt.type === "GET") {
                    expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.processData).toEqual(false);
                    opt.success({count: 100000});
                } else if (opt.type === "POST") {
                    expect(opt.url).toEqual('/_api/cursor');
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.data).toEqual(JSON.stringify({
                        query: "FOR x in @@collection LIMIT @offset, @count RETURN x",
                        bindVars: {
                            "@collection": colid,
                            "offset": 10,
                            "count": 10
                        }
                    }));
                    opt.success({result: [
                        {_id: 1, _rev: 2, _key: 4},
                        {_id: 2, _rev: 2, _key: 4},
                        {_id: 3, _rev: 2, _key: 4}
                    ]
                    });
                }
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            result = col.getDocuments(colid, currpage);
            expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10000);
        });

        it("should getDocuments with initial draw", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                if (opt.type === "GET") {
                    expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.processData).toEqual(false);
                    opt.success({count: 0});
                } else if (opt.type === "POST") {
                    expect(opt.url).toEqual('/_api/cursor');
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.data).toEqual(JSON.stringify({
                        query: "FOR x in @@collection SORT TO_NUMBER(x._key) == 0 " +
                            "? x._key : TO_NUMBER(x._key) LIMIT @offset, @count RETURN x",
                        bindVars: {
                            "@collection": colid,
                            "offset": 10,
                            "count": 10
                        }
                    }));
                    opt.success({result: [
                        {_id: 1, _rev: 2, _key: 4},
                        {_id: 2, _rev: 2, _key: 4},
                        {_id: 3, _rev: 2, _key: 4}
                    ]
                    });
                }
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            result = col.getDocuments(colid, currpage);
            expect(window.documentsView.initTable).toHaveBeenCalled();
        });


        it("should getDocuments with exceeding sort count", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                if (opt.type === "GET") {
                    expect(opt.url).toEqual("/_api/collection/" + colid + "/count");
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.processData).toEqual(false);
                    opt.success({count: 100000});
                } else if (opt.type === "POST") {
                    expect(opt.url).toEqual('/_api/cursor');
                    expect(opt.contentType).toEqual("application/json");
                    expect(opt.cache).toEqual(false);
                    expect(opt.async).toEqual(false);
                    expect(opt.data).toEqual(JSON.stringify({
                        query: "FOR x in @@collection LIMIT @offset, @count RETURN x",
                        bindVars: {
                            "@collection": colid,
                            "offset": 10,
                            "count": 10
                        }
                    }));
                    opt.success({result: [
                        {_id: 1, _rev: 2, _key: 4},
                        {_id: 2, _rev: 2, _key: 4},
                        {_id: 3, _rev: 2, _key: 4}
                    ]
                    });
                }
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            result = col.getDocuments(colid, currpage);
            expect(window.documentsView.renderPagination).toHaveBeenCalledWith(10000);
        });

        it("should sorted getFilteredDocuments with empty filter", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual('/_api/cursor');
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify({
                    query: "FOR u in @@collection SORT " +
                        "TO_NUMBER(u._key) == 0 ? u._key : TO_NUMBER(u._key)" +
                        " LIMIT @offset, @count RETURN u",
                    bindVars: {
                        "@collection": "12345", "count": 10, "offset": 10
                    },
                    options: {
                        fullCount: true
                    }
                }));
                opt.success({result: [
                    {_id: 1, _rev: 2, _key: 4},
                    {_id: 2, _rev: 2, _key: 4},
                    {_id: 3, _rev: 2, _key: 4}
                ], extra: {fullCount: 10}
                });
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.arangoDocumentsStore, "add");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            col.documentsCount = 100;
            result = col.getFilteredDocuments(colid, currpage, [], []);
            expect(window.documentsView.renderPagination).toHaveBeenCalled();
        });

        it("should sorted getFilteredDocuments with empty filter and empty result", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual('/_api/cursor');
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify({
                    query: "FOR u in @@collection " +
                        "SORT TO_NUMBER(u._key) == 0 ? u._key : TO_NUMBER(u._key)" +
                        " LIMIT @offset, @count RETURN u",
                    bindVars: {
                        "@collection": "12345", "count": 10, "offset": 10
                    },
                    options: {
                        fullCount: true
                    }
                }));
                opt.success({result: [
                    {_id: 1, _rev: 2, _key: 4},
                    {_id: 2, _rev: 2, _key: 4},
                    {_id: 3, _rev: 2, _key: 4}
                ], extra: {fullCount: 0}
                });
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.arangoDocumentsStore, "add");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            col.documentsCount = 100;
            result = col.getFilteredDocuments(colid, currpage, [], []);
            expect(window.documentsView.initTable).toHaveBeenCalled();
        });

        it("should sorted getFilteredDocuments with filter", function () {
            var colid = "12345", currpage = "2", result;
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual('/_api/cursor');
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify({
                    query: "FOR u in @@collection FILTER u.NAME = " +
                        "@@name &&  u.AGE > @age SORT TO_NUMBER(u._key) == 0" +
                        " ? u._key : TO_NUMBER(u._key)" +
                        " LIMIT @offset, @count RETURN u",
                    bindVars: {
                        "@collection": "12345",
                        count: 10,
                        offset: 10,
                        name: "Heinz",
                        age: 4
                    },
                    options: {
                        fullCount: true
                    }
                }));
                opt.success({result: [
                    {_id: 1, _rev: 2, _key: 4},
                    {_id: 2, _rev: 2, _key: 4},
                    {_id: 3, _rev: 2, _key: 4}
                ], extra: {fullCount: 10}
                });
            });
            spyOn(window.arangoDocumentsStore, "reset");
            spyOn(window.arangoDocumentsStore, "add");
            spyOn(window.documentsView, "drawTable");
            spyOn(window.documentsView, "renderPagination");
            spyOn(window.documentsView, "initTable");
            col.documentsCount = 100;
            result = col.getFilteredDocuments(
                colid, currpage, [' u.NAME = @@name', ' u.AGE > @age'],
                {name: "Heinz", age: 4});
            expect(window.documentsView.renderPagination).toHaveBeenCalled();
        });

        it("should getStatisticsHistory", function () {
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual("/_admin/history");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.beforeSend).toNotBe(undefined);
                expect(opt.data).toEqual(JSON.stringify(
                        {startDate: 15000, endDate: 18000, figures: ["bytesSend, totalTime"]}));
                opt.success({result: [
                    {bytesSend: 1, totalTime: 2, time: 4},
                    {bytesSend: 2, totalTime: 2, time: 4},
                    {bytesSend: 3, totalTime: 2, time: 4}
                ]
                });
            });
            var result = col.getStatisticsHistory(
                {startDate: 15000, endDate: 18000, figures: ["bytesSend, totalTime"]});
            expect(JSON.stringify(col.history)).toEqual(JSON.stringify([
                {bytesSend: 1, totalTime: 2, time: 4},
                {bytesSend: 2, totalTime: 2, time: 4},
                {bytesSend: 3, totalTime: 2, time: 4}
            ]));
        });

        it("should getStatisticsHistory from remote dispatcher and get an error", function () {
            delete col.history;
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual("endpoint/_admin/history");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.beforeSend).toNotBe(undefined);
                expect(opt.data).toEqual(JSON.stringify({startDate: 15000, endDate: 18000,
                        figures: ["bytesSend, totalTime"]}));
                opt.error();
            });
            var result = col.getStatisticsHistory({server: {endpoint: "endpoint", isDBServer: false,
                addAuth: "authKey"},
                startDate: 15000, endDate: 18000, figures: ["bytesSend, totalTime"]});
            expect(col.history).toEqual(undefined);
        });

        it("should getStatisticsHistory from remote dbserver and get an error", function () {
            delete col.history;
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.type).toEqual("POST");
                expect(opt.url).toEqual("endpoint/_admin/history?DBserver=anotherserver");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                expect(opt.beforeSend).toNotBe(undefined);
                expect(opt.data).toEqual(JSON.stringify({startDate: 15000, endDate: 18000,
                        figures: ["bytesSend, totalTime"]}));
                opt.error();
            });
            var result = col.getStatisticsHistory({server: {endpoint: "endpoint", isDBServer: true,
                addAuth: "authKey", target: "anotherserver"}, startDate: 15000, endDate: 18000,
                figures: ["bytesSend, totalTime"]});
            expect(col.history).toEqual(undefined);
        });
    });

}());

