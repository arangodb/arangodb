/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global arangoHelper*/

(function () {
    "use strict";

    describe("The document view", function () {

        var model, view, div;

        beforeEach(function () {
            div = document.createElement("div");
            div.id = "content";
            document.body.appendChild(div);

            model = new window.arangoDocument({
                _key: "123",
                _id: "v/123",
                _rev: "123",
                name: "alice"
            });

            view = new window.DocumentView({
                collection: new window.arangoDocument()
            });

            view.collection.add(model);
            view.render();
        });

        afterEach(function () {
            document.body.removeChild(div);
        });

        it("assert the basics", function () {
            expect(view.colid).toEqual(0);
            expect(view.colid).toEqual(0);
            expect(view.events).toEqual({
                "click #saveDocumentButton": "saveDocument",
                "dblclick #documentEditor tr" : "addProperty"
            });
        });

        it("should copy a model into editor", function () {
            spyOn(view.editor, "set");
            view.fillEditor();
            expect(view.editor.set).toHaveBeenCalled();
        });

        it("should save type document", function () {
            view.type = 'document';
            spyOn(view.collection, "saveDocument").andReturn(true);
            view.saveDocument();
            expect(view.collection.saveDocument).toHaveBeenCalled();
        });

        it("should save type edge", function () {
            view.type = 'edge';
            spyOn(view.collection, "saveEdge").andReturn(true);
            view.saveDocument();
            expect(view.collection.saveEdge).toHaveBeenCalled();
        });

        it("should save type edge and return error", function () {
            view.type = 'edge';
            spyOn(view.collection, "saveEdge").andReturn(false);
            spyOn(arangoHelper, "arangoError");
            view.saveDocument();
            expect(arangoHelper.arangoError).toHaveBeenCalled();
        });

        it("should save type document and return error", function () {
            view.type = 'document';
            spyOn(view.collection, "saveDocument").andReturn(false);
            spyOn(arangoHelper, "arangoError");
            view.saveDocument();
            expect(arangoHelper.arangoError).toHaveBeenCalled();
        });

        it("check document view type check positive", function () {
            view.collection.add(model);
            spyOn(view, "fillEditor");

            spyOn(view.collection, "getEdge").andReturn(true);
            spyOn(view.collection, "getDocument").andReturn(true);
            var result = view.typeCheck('edge');
            expect(result).toEqual(true);
            expect(view.collection.getEdge).toHaveBeenCalled();
            result = view.typeCheck('document');
            expect(view.collection.getDocument).toHaveBeenCalled();
            expect(result).toEqual(true);
        });

        it("check document view type check negative", function () {
            view.collection.add(model);
            spyOn(view, "fillEditor");

            var result = view.typeCheck('easddge');
            expect(result).not.toEqual(true);

            result = view.typeCheck(false);
            expect(result).not.toEqual(true);

            result = view.typeCheck(123);
            expect(result).not.toEqual(true);
        });

        it("should remove readonly keys", function () {
            var object = {
                    hello: 123,
                    wrong: true,
                    _key: "123",
                    _rev: "adasda",
                    _id: "paosdjfp1321"
                },
                shouldObject = {
                    hello: 123,
                    wrong: true
                },
                result = view.removeReadonlyKeys(object);

            expect(result).toEqual(shouldObject);
        });

        it("should modify the breadcrumb", function () {
            var bar = document.createElement("div"),
                emptyBar = document.createElement("div");
            bar.id = 'transparentHeader';

            view.breadcrumb();
            expect(emptyBar).not.toBe(bar);
        });

        it("escaped", function () {
            expect(view.escaped('&<>"\'')).toEqual("&amp;&lt;&gt;&quot;&#39;");
        });


    });

}());
