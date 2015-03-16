/*jshint browser: true */
/*jshint unused: false */
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
                "click #deleteDocumentButton" : "deleteDocumentModal",
                "click #confirmDeleteDocument" : "deleteDocument",
                "click #document-from" : "navigateToDocument",
                "click #document-to" : "navigateToDocument",
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

        it("should addProperty in existing json document", function () {

            var eDummy = {
                    currentTarget :  {
                        cells : [
                            "",
                            "",
                            {
                                childNodes : [
                                    {
                                        childNodes : [
                                            {
                                                childNodes : [
                                                    {
                                                        childNodes : [
                                                            "",
                                                            {
                                                                childNodes : [
                                                                    {
                                                                        textContent : "bla"

                                                                    }
                                                                ]
                                                            }
                                                        ]
                                                    }
                                                ]
                                            }
                                        ]
                                    }
                                ]
                            }
                        ]
                    }
                }, editorDummy  = {

                node : {
                    search : function () {

                    }
                }


            }, nodeDummy1 =  {elem : "value", node : {_onInsertAfter : function () {}}},
            nodeDummy2 =  {elem : "field", node : {_onInsertAfter : function () {}}} ;
            spyOn(editorDummy.node, "search").andReturn([
                nodeDummy1,
                nodeDummy2,
                nodeDummy1
            ]);
            spyOn(nodeDummy1.node, "_onInsertAfter");
            spyOn(nodeDummy2.node, "_onInsertAfter");
            view.editor = editorDummy;

            view.addProperty(eDummy);


            expect(editorDummy.node.search).toHaveBeenCalledWith("bla");
            expect(nodeDummy1.node._onInsertAfter).not.toHaveBeenCalled();
            expect(nodeDummy2.node._onInsertAfter).toHaveBeenCalledWith(
                undefined, undefined, "auto");

        });



        it("should addProperty in existing json document in first position", function () {

            var eDummy = {
                    currentTarget :  {
                        cells : [
                            "",
                            "",
                            {
                                childNodes : [
                                    {
                                        childNodes : [
                                            {
                                                childNodes : [
                                                    {
                                                        childNodes : [
                                                            "",
                                                            {
                                                                childNodes : [
                                                                    {
                                                                        textContent : "object"

                                                                    }
                                                                ]
                                                            }
                                                        ]
                                                    }
                                                ]
                                            }
                                        ]
                                    }
                                ]
                            }
                        ]
                    }
                }, editorDummy  = {

                    node : {
                        search : function () {

                        },
                        childs : [
                            {
                                focus : function () {},
                                _onInsertBefore : function () {

                                }
                            }
                        ]
                    },
                    get : function () {

                    },
                    set : function () {

                    }


                }, nodeDummy1 =  {elem : "value", _onInsertAfter : function () {}},
                nodeDummy2 =  {elem : "value", _onInsertAfter : function () {}} ;
            spyOn(editorDummy.node, "search").andReturn([
                nodeDummy1,
                nodeDummy2,
                nodeDummy1
            ]);
            spyOn(nodeDummy1, "_onInsertAfter");
            spyOn(nodeDummy2, "_onInsertAfter");

            spyOn(editorDummy, "get").andReturn([
                nodeDummy1,
                nodeDummy2,
                nodeDummy1
            ]);

            spyOn(editorDummy, "set");
            spyOn(editorDummy.node.childs[0], "_onInsertBefore");
            spyOn(editorDummy.node.childs[0], "focus");

            view.editor = editorDummy;

            view.addProperty(eDummy);


            expect(editorDummy.node.search).not.toHaveBeenCalled();
            expect(editorDummy.node.childs[0].focus).not.toHaveBeenCalled();
            expect(editorDummy.node.childs[0]._onInsertBefore).toHaveBeenCalledWith(
                undefined, undefined, "auto");
            expect(editorDummy.get).toHaveBeenCalled();
            expect(editorDummy.set).not.toHaveBeenCalled();


        });


        it("should addProperty in new json document in first position", function () {

            var eDummy = {
                    currentTarget :  {
                        cells : [
                            "",
                            "",
                            {
                                childNodes : [
                                    {
                                        childNodes : [
                                            {
                                                childNodes : [
                                                    {
                                                        childNodes : [
                                                            "",
                                                            {
                                                                childNodes : [
                                                                    {
                                                                        textContent : "object"

                                                                    }
                                                                ]
                                                            }
                                                        ]
                                                    }
                                                ]
                                            }
                                        ]
                                    }
                                ]
                            }
                        ]
                    }
                }, editorDummy  = {

                    node : {
                        search : function () {

                        },
                        childs : [
                            {
                                focus : function () {},
                                _onInsertBefore : function () {

                                }
                            }
                        ]
                    },
                    get : function () {

                    },
                    set : function () {

                    }


                }, nodeDummy1 =  {elem : "value", _onInsertAfter : function () {}},
                nodeDummy2 =  {elem : "value", _onInsertAfter : function () {}} ;
            spyOn(editorDummy.node, "search").andReturn([
                nodeDummy1,
                nodeDummy2,
                nodeDummy1
            ]);
            spyOn(nodeDummy1, "_onInsertAfter");
            spyOn(nodeDummy2, "_onInsertAfter");

            spyOn(editorDummy, "get").andReturn(undefined);

            spyOn(editorDummy, "set");
            spyOn(editorDummy.node.childs[0], "_onInsertBefore");
            spyOn(editorDummy.node.childs[0], "focus");

            view.editor = editorDummy;

            view.addProperty(eDummy);


            expect(editorDummy.node.search).not.toHaveBeenCalled();
            expect(editorDummy.node.childs[0].focus).toHaveBeenCalledWith("field");
            expect(editorDummy.node.childs[0]._onInsertBefore).not.toHaveBeenCalled();
            expect(editorDummy.get).toHaveBeenCalled();
            expect(editorDummy.set).toHaveBeenCalledWith({
                "": ""
            });

        });

    });

}());
