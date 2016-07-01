/* jshint browser: true */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global arangoHelper, $*/

(function () {
  'use strict';

  describe('The document view', function () {
    var model, view, div, collection, setEditorInput;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);

      model = new window.arangoDocumentModel({
        _key: '123',
        _id: 'v/123',
        _rev: '123',
        name: 'alice'
      });

      collection = new window.arangoDocument();

      view = new window.DocumentView({
        collection: collection
      });

      setEditorInput = function (text) {
        view.editor.setMode('code');
        view.editor.setText(text);
        $('.ace_editor').focusout();
      };

      collection.add(model);
      view.render();
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('assert the basics', function () {
      expect(view.colid).toEqual(0);
      expect(view.docid).toEqual(0);
      expect(view.events).toEqual({
        'click #saveDocumentButton': 'saveDocument',
        'click #deleteDocumentButton': 'deleteDocumentModal',
        'click #confirmDeleteDocument': 'deleteDocument',
        'click #document-from': 'navigateToDocument',
        'click #document-to': 'navigateToDocument'
      });
    });

    describe('setting of type', function () {
      var typeCheck, saveCheck;

      beforeEach(function () {
        typeCheck = {
          result: true
        };
        saveCheck = {
          result: true
        };
        spyOn(collection, 'getEdge').andCallFake(function () {
          return typeCheck.result;
        });
        spyOn(collection, 'getDocument').andCallFake(function () {
          return typeCheck.result;
        });
        spyOn(collection, 'saveDocument').andCallFake(function () {
          return saveCheck.result;
        });
        spyOn(collection, 'saveEdge').andCallFake(function () {
          return saveCheck.result;
        });
      });

      describe('document', function () {
        beforeEach(function () {
          view.setType('document');
        });

        afterEach(function () {
          expect(collection.getDocument).toHaveBeenCalledWith(
            view.colid,
            view.docid
          );
          expect(collection.getEdge).not.toHaveBeenCalled();
        });

        it('should copy a model into editor', function () {
          spyOn(view.editor, 'set');
          view.setType('document');
          expect(view.editor.set).toHaveBeenCalledWith({
            name: 'alice'
          });
        });

        it('should render the internal information', function () {
          expect($('#document-type').text()).toEqual('Document: ');
          expect($('#document-id').text()).toEqual(model.get('_id'));
          expect($('#document-key').text()).toEqual(model.get('_key'));
          expect($('#document-rev').text()).toEqual(model.get('_rev'));
        });

        it('should not be possible to resave an unchanged document', function () {
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).not.toHaveBeenCalled();
          expect(collection.saveEdge).not.toHaveBeenCalled();
        });

        it('should not be possible to save invalid json', function () {
          collection.saveDocument.reset();
          collection.saveEdge.reset();

          setEditorInput('test');
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).not.toHaveBeenCalled();
          expect(collection.saveEdge).not.toHaveBeenCalled();

          collection.saveDocument.reset();
          collection.saveEdge.reset();

          setEditorInput('{te: st}');
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).not.toHaveBeenCalled();
          expect(collection.saveEdge).not.toHaveBeenCalled();
        });

        it('should be possible to save valid JSON', function () {
          var doc = {name: 'Bob'};
          setEditorInput(JSON.stringify(doc));
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).toHaveBeenCalledWith(
            view.colid,
            view.docid,
            JSON.stringify(doc)
          );
          expect(collection.saveEdge).not.toHaveBeenCalled();
        });

        it('should be possible to save valid JSON with colons', function () {
          var doc = '{"name": "Bob:"}';
          setEditorInput(doc);
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).toHaveBeenCalledWith(
            view.colid,
            view.docid,
            JSON.stringify({name: 'Bob:'})
          );
          expect(collection.saveEdge).not.toHaveBeenCalled();
        });

        it('should insert the error in the notification box', function () {
          saveCheck.result = false;
          spyOn(arangoHelper, 'arangoError');
          var doc = {name: 'Bob'};
          setEditorInput(JSON.stringify(doc));
          $('#saveDocumentButton').click();
          expect(arangoHelper.arangoError).toHaveBeenCalled();
        });
      });

      describe('edge', function () {
        beforeEach(function () {
          model.set('_from', 'v/123');
          model.set('_to', 'v/321');
          view.setType('edge');
        });

        afterEach(function () {
          expect(collection.getEdge).toHaveBeenCalledWith(
            view.colid,
            view.docid
          );
          expect(collection.getDocument).not.toHaveBeenCalled();
        });

        it('should copy a model into editor', function () {
          spyOn(view.editor, 'set');
          view.setType('edge');
          expect(view.editor.set).toHaveBeenCalledWith({
            name: 'alice'
          });
        });

        it('should render the internal information', function () {
          var fromSplit = model.get('_from').split('/');
          var toSplit = model.get('_to').split('/');
          var fromRef = 'collection/'
          + encodeURIComponent(fromSplit[0]) + '/'
          + encodeURIComponent(fromSplit[1]);
          var toRef = 'collection/'
          + encodeURIComponent(toSplit[0]) + '/'
          + encodeURIComponent(toSplit[1]);
          expect($('#document-type').text()).toEqual('Edge: ');
          expect($('#document-id').text()).toEqual(model.get('_id'));
          expect($('#document-key').text()).toEqual(model.get('_key'));
          expect($('#document-rev').text()).toEqual(model.get('_rev'));
          expect($('#document-from').text()).toEqual(model.get('_from'));
          expect($('#document-from').attr('documentLink')).toEqual(fromRef);
          expect($('#document-to').attr('documentLink')).toEqual(toRef);
        });

        it('should not be possible to resave an unchanged document', function () {
          $('#saveDocumentButton').click();
          expect(collection.saveDocument).not.toHaveBeenCalled();
          expect(collection.saveEdge).not.toHaveBeenCalled();
        });

        it('should be possible to save a valid JSON', function () {
          var doc = {name: 'Bob'};
          setEditorInput(JSON.stringify(doc));
          $('#saveDocumentButton').click();
          expect(collection.saveEdge).toHaveBeenCalledWith(
            view.colid,
            view.docid,
            JSON.stringify(doc)
          );
          expect(collection.saveDocument).not.toHaveBeenCalled();
        });

        it('should insert the error in the notification box', function () {
          saveCheck.result = false;
          spyOn(arangoHelper, 'arangoError');
          var doc = {name: 'Bob'};
          setEditorInput(JSON.stringify(doc));
          $('#saveDocumentButton').click();
          expect(arangoHelper.arangoError).toHaveBeenCalled();
        });
      });

      describe('invalid type', function () {
        it('should be rejected', function () {
          spyOn(view, 'fillInfo');
          spyOn(view, 'fillEditor');
          var result = view.setType('easddge');
          expect(result).not.toEqual(true);
          expect(view.fillInfo).not.toHaveBeenCalled();
          expect(view.fillEditor).not.toHaveBeenCalled();
        });
      });
    });

    it('should remove readonly keys', function () {
      var object = {
          hello: 123,
          wrong: true,
          _key: '123',
          _rev: 'adasda',
          _id: 'paosdjfp1321'
        },
        shouldObject = {
          hello: 123,
          wrong: true
        },
        result = view.removeReadonlyKeys(object);

      expect(result).toEqual(shouldObject);
    });

    it('should modify the breadcrumb', function () {
      var bar = document.createElement('div'),
        emptyBar = document.createElement('div');
      bar.id = 'transparentHeader';

      view.breadcrumb();
      expect(emptyBar).not.toBe(bar);
    });

    it('escaped', function () {
      expect(view.escaped('&<>"\'')).toEqual('&amp;&lt;&gt;&quot;&#39;');
    });
  });
}());
