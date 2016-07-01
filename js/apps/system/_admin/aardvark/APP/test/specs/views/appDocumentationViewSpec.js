/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, jasmine*/
/* global window, document, hljs, $, require*/

(function () {
  'use strict';

  describe('The App documentation view', function () {
    var div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('should define the swagger UI on create', function () {
      var fakeURL = '/fake/url',
        fakeKey = 'fakeKey',
        view,
        internal = require('internal');
      spyOn(internal.arango, 'databasePrefix').andReturn(fakeURL);
      spyOn(window, 'SwaggerUi');
      view = new window.AppDocumentationView({
        key: fakeKey
      });
      expect(internal.arango.databasePrefix).toHaveBeenCalledWith(
        '/_admin/aardvark/docu/' + fakeKey
      );
      expect(window.SwaggerUi).toHaveBeenCalledWith({
        discoveryUrl: fakeURL,
        apiKey: false,
        dom_id: 'swagger-ui-container',
        supportHeaderParams: true,
        supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
        onComplete: jasmine.any(Function),
        onFailure: jasmine.any(Function),
        docExpansion: 'none'
      });
    });

    describe('after initialize', function () {
      var view, swaggerDummy;

      beforeEach(function () {
        swaggerDummy = {
          load: function () {
            throw 'should always be spied upon';
          }
        };
        spyOn(window, 'SwaggerUi').andCallFake(function (opts) {
          swaggerDummy.onComplete = opts.onComplete;
          swaggerDummy.onFailure = opts.onFailure;
          return swaggerDummy;
        });
        view = new window.AppDocumentationView();
      });

      it('swagger onComplete should highlight blocks', function () {
        spyOn(hljs, 'highlightBlock');
        var pre = document.createElement('pre'),
          code = document.createElement('code');
        document.body.appendChild(pre);
        pre.appendChild(code);
        swaggerDummy.onComplete();
        expect(hljs.highlightBlock).toHaveBeenCalledWith($(code)[0]);
        expect(hljs.highlightBlock.callCount).toEqual(1);
        document.body.removeChild(pre);
      });

      it('should create appropriate onFailure info', function () {
        var dummyDiv = document.createElement('div'),
          dummyStrong = document.createElement('strong'),
          dummyBr = document.createElement('br'),
          dummyText = document.createTextNode('dummy'),
          fakeData = {fake: 'data'};
        spyOn(document, 'createElement').andCallFake(function (name) {
          switch (name) {
            case 'div':
              return dummyDiv;
            case 'strong':
              return dummyStrong;
            case 'br':
              return dummyBr;
            default:
              throw 'creating unwanted element';
          }
        });
        spyOn(document, 'createTextNode').andReturn(dummyText);
        spyOn(dummyDiv, 'appendChild');
        spyOn(dummyStrong, 'appendChild');
        swaggerDummy.onFailure(fakeData);
        expect(dummyDiv.appendChild).toHaveBeenCalledWith(dummyStrong);
        expect(dummyDiv.appendChild).toHaveBeenCalledWith(dummyBr);
        expect(dummyDiv.appendChild).toHaveBeenCalledWith(dummyText);
        expect(dummyStrong.appendChild).toHaveBeenCalledWith(dummyText);
        expect(document.createTextNode)
          .toHaveBeenCalledWith('Sorry the code is not documented properly');
        expect(document.createTextNode).toHaveBeenCalledWith(JSON.stringify(fakeData));
      });

      it('should load the SwaggerUI on render', function () {
        spyOn(swaggerDummy, 'load');
        view.render();
        expect(swaggerDummy.load).toHaveBeenCalled();
      });
    });
  });
}());
