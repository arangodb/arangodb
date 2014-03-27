/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function () {
    "use strict";

    describe("arangoCollections", function () {

        var col;

        beforeEach(function () {
            col = new window.arangoCollections();
            window.collectionsView = new window.CollectionsView({
                collection: col
            });
            window.arangoCollectionsStore = col;
        });

        it("should translate various Status", function () {
            expect(col.translateStatus(0)).toEqual("corrupted");
            expect(col.translateStatus(1)).toEqual("new born collection");
            expect(col.translateStatus(2)).toEqual("unloaded");
            expect(col.translateStatus(3)).toEqual("loaded");
            expect(col.translateStatus(4)).toEqual("in the process of being unloaded");
            expect(col.translateStatus(5)).toEqual("deleted");
            expect(col.translateStatus(6)).toEqual("loading");
            expect(col.translateStatus(7)).toEqual(undefined);
        });

        it("should translate various typePictures", function () {
            expect(col.translateTypePicture('document')).toEqual("img/icon_document.png");
            expect(col.translateTypePicture('edge')).toEqual("img/icon_node.png");
            expect(col.translateTypePicture('unknown')).toEqual("img/icon_unknown.png");
            expect(col.translateTypePicture("default")).toEqual("img/icon_arango.png");
        });

        it("should parse a response", function () {
            var response = {collections: [
                {name: "_aSystemCollection", type: 2, status: 2}
            ]};
            col.parse(response);
            expect(response.collections[0].isSystem).toEqual(true);
            expect(response.collections[0].type).toEqual("document (system)");
            expect(response.collections[0].status).toEqual("unloaded");
            expect(response.collections[0].picture).toEqual("img/icon_arango.png");
        });

        it("should getPosition for loaded documents sorted by type", function () {

            col.models = [
                new window.arangoCollectionModel({isSystem: true, name: "Heinz Herbert"}),
                new window.arangoCollectionModel({type: 'edge', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({type: 'document', name: "Heinz Herbert Gustav"}),
                new window.arangoCollectionModel({type: 'document', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({status: 'loaded', name: "Heinz Herbert Karl"}),
                new window.arangoCollectionModel({status: 'unloaded', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({name: "Heinbert"})
            ];
            col.searchOptions = {
                searchPhrase: "Heinz Herbert",
                includeSystem: false,
                includeDocument: true,
                includeEdge: false,
                includeLoaded: true,
                includeUnloaded: false,
                sortBy: 'type',
                sortOrder: 1
            }
            var result = col.getPosition("Heinz Herbert");
            expect(result.prev).not.toBe(null);
            expect(result.next).not.toBe(null);
        });

        it("should getPosition for unloaded edges documents sorted by type", function () {

            col.models = [
                new window.arangoCollectionModel({isSystem: true, name: "Heinz Herbert"}),
                new window.arangoCollectionModel({type: 'edge', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({type: 'document', name: "Heinz Herbert Gustav"}),
                new window.arangoCollectionModel({type: 'document', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({status: 'loaded', name: "Heinz Herbert Karl"}),
                new window.arangoCollectionModel({status: 'unloaded', name: "Heinz Herbert"}),
                new window.arangoCollectionModel({name: "Heinbert"})
            ];
            col.searchOptions = {
                searchPhrase: "Heinz Herbert",
                includeSystem: true,
                includeDocument: false,
                includeEdge: true,
                includeLoaded: false,
                includeUnloaded: true,
                sortBy: 'name',
                sortOrder: 1
            }
            var result = col.getPosition("Heinz Herbert");
            expect(result.prev).not.toBe(null);
            expect(result.next).not.toBe(null);
        });

        it("should get and index and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index/?collection=" + id);
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.getIndex(12345)).toEqual("success");

        });

        it("should get and index and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index/?collection=" + id);
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.getIndex(12345)).toEqual("error");

        });

        it("should create and index and succeed", function () {
            var collection = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index?collection=" + collection);
                expect(opt.type).toEqual("POST");
                expect(opt.cache).toEqual(false);
                expect(opt.data).toEqual('{"ab":"CD"}');
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.createIndex(collection, {ab: 'CD'})).toEqual(true);

        });

        it("should create and index and fail", function () {
            var collection = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index?collection=" + collection);
                expect(opt.type).toEqual("POST");
                expect(opt.cache).toEqual(false);
                expect(opt.data).toEqual('{"ab":"CD"}');
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.createIndex(collection, {ab: 'CD'})).toEqual("error");

        });


        it("should delete and index and succeed", function () {
            var collection = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index/" + collection + "/1");
                expect(opt.type).toEqual("DELETE");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.deleteIndex(collection, 1)).toEqual(true);

        });

        it("should delete and index and fail", function () {
            var collection = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/index/" + collection + "/1");
                expect(opt.type).toEqual("DELETE");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.deleteIndex(collection, 1)).toEqual(false);

        });

        it("should get Properties and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.getProperties(12345)).toEqual("success");

        });

        it("should get Properties and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.getProperties(12345)).toEqual("error");

        });

        it("should get Figures and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/figures");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.getFigures(12345)).toEqual("success");

        });

        it("should get Figures and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/figures");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.getFigures(12345)).toEqual("error");

        });

        it("should get Revision and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/revision");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            expect(col.getRevision(12345)).toEqual("success");

        });

        it("should get Revision and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/revision");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error("error");
            });
            expect(col.getRevision(12345)).toEqual("error");

        });

        it("should create a Collection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection");
                expect(opt.type).toEqual("POST");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {
                        name: "heinz", waitForSync: true, isSystem: true,
                        type: 3, numberOfShards: 3, shardKeys: ["a", "b", "c"]
                    }
                ));
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            var result = col.newCollection("heinz", true, true, 2, 3, 3, ["a", "b", "c"]);
            expect(result.data).toEqual("success");
            expect(result.status).toEqual(true);

        });

        it("should create a Collection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection");
                expect(opt.type).toEqual("POST");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {
                        name: "heinz", waitForSync: true, isSystem: true,
                        type: 3, numberOfShards: 3, shardKeys: ["a", "b", "c"]
                    }
                ));
                expect(opt.async).toEqual(false);
                opt.error({responseText: '{ "errorMessage" : "went wrong"}'});
            });
            var result = col.newCollection("heinz", true, true, 2, 3, 3, ["a", "b", "c"]);
            expect(result.errorMessage).toEqual("went wrong");
            expect(result.status).toEqual(false);
        });


        it("should renameCollection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/rename");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {name: "newName"}
                ));
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            var result = col.renameCollection(12345, "newName");
            expect(result).toEqual(true);

        });

        it("should renameCollection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/rename");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {name: "newName"}
                ));
                expect(opt.async).toEqual(false);
                opt.error({responseText: '{ "errorMessage" : "went wrong"}'});
            });
            var result = col.renameCollection(12345, "newName");
            expect(result).toEqual("went wrong");
        });

        it("should renameCollection and fail with unparseable message", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/rename");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {name: "newName"}
                ));
                expect(opt.async).toEqual(false);
                opt.error(["otto"]);
            });
            var result = col.renameCollection(12345, "newName");
            expect(result).toEqual(false);
        });

        it("should changeCollection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {"waitForSync": false, "journalSize": 4}
                ));
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            var result = col.changeCollection(id, false, 4);
            expect(result).toEqual(true);

        });

        it("should changeCollection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {"waitForSync": false, "journalSize": 4}
                ));
                expect(opt.async).toEqual(false);
                opt.error({responseText: '{ "errorMessage" : "went wrong"}'});
            });
            var result = col.changeCollection(id, false, 4);
            expect(result).toEqual("went wrong");

        });

        it("should changeCollection and fail with nonparseable message", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/properties");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                expect(opt.contentType).toEqual("application/json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify(
                    {"waitForSync": false, "journalSize": 4}
                ));
                expect(opt.async).toEqual(false);
                opt.error(["otto"]);
            });
            var result = col.changeCollection(id, false, 4);
            expect(result).toEqual(false);

        });


        it("should deleteCollection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id);
                expect(opt.type).toEqual("DELETE");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success("success");
            });
            spyOn(window.collectionsView, "render");
            var result = col.deleteCollection(id);
            expect(window.collectionsView.render).toHaveBeenCalled();
            expect(result).toEqual(true);

        });

        it("should deleteCollection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id);
                expect(opt.type).toEqual("DELETE");
                expect(opt.cache).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error();
            });
            var result = col.deleteCollection(id);
            expect(result).toEqual(false);

        });

        it("should loadCollection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/load");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                opt.success("success");
            });
            spyOn(arangoHelper, "arangoNotification");
            spyOn(window.collectionsView, "render");
            spyOn(window.arangoCollectionsStore, "fetch").andCallFake(function (o) {
                o.success();
            });
            var result = col.loadCollection(id);
            expect(arangoHelper.arangoNotification).toHaveBeenCalledWith('Collection loaded');
            expect(window.collectionsView.render).toHaveBeenCalled();
        });
        it("should loadCollection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/load");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                opt.error();
            });
            spyOn(arangoHelper, "arangoError");
            var result = col.loadCollection(id);
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Collection error');

        });

        it("should unloadCollection and succeed", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/unload");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                opt.success("success");
            });
            spyOn(arangoHelper, "arangoNotification");
            spyOn(window.collectionsView, "render");
            spyOn(window.arangoCollectionsStore, "fetch").andCallFake(function (o) {
                o.success();
            });
            var result = col.unloadCollection(id);
            expect(arangoHelper.arangoNotification).toHaveBeenCalledWith('Collection unloaded');
            expect(window.collectionsView.render).toHaveBeenCalled();
        });
        it("should unloadCollection and fail", function () {
            var id = "12345";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/collection/" + id + "/unload");
                expect(opt.type).toEqual("PUT");
                expect(opt.cache).toEqual(false);
                opt.error();
            });
            spyOn(arangoHelper, "arangoError");
            var result = col.unloadCollection(id);
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Collection error');

        });

    });

}());

