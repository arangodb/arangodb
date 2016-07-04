/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('The current database', function () {
    var model, myKey, ajaxVerify, server_result, e, v;

    beforeEach(function () {
      myKey = 'graph';
      e = 'e';
      v = 'v';
      server_result = {
        graph: {
          _id: '_graph/' + myKey,
          _key: myKey,
          _rev: '123541',
          edges: e,
          vertices: v
        },
        code: 201,
        error: false
      };
      ajaxVerify = function () {
        return undefined;
      };
      spyOn($, 'ajax').andCallFake(function (opt) {
        ajaxVerify(opt);
        if (opt.success) {
          opt.success(server_result);
        }
      });
      model = new window.Graph({
        _key: myKey,
        vertices: v,
        edges: e
      });
    });

    it('should request /_api/graph on save', function () {
      ajaxVerify = function (opt) {
        expect(opt.url).toEqual('/_api/gharial');
        expect(opt.type).toEqual('POST');
      };
      model.save();
      expect($.ajax).toHaveBeenCalled();
    });
    it('should store the attributes in the model', function () {
      var id = '_graph/' + myKey,
        rev = '12345';
      server_result.graph._id = id;
      server_result.graph._rev = rev;
      model.save();
      expect(model.get('_id')).toEqual(id);
      expect(model.get('_rev')).toEqual(rev);
      expect(model.get('_key')).toEqual(myKey);
      expect(model.get('edges')).toEqual(e);
      expect(model.get('vertices')).toEqual(v);
      expect(model.get('error')).toBeUndefined();
      expect(model.get('code')).toBeUndefined();
      expect(model.get('graph')).toBeUndefined();
    });

    it('should be able to parse the raw graph', function () {
      var id = '_graph/' + myKey,
        rev = '12345',
        tmpResult = server_result.graph;
      server_result = tmpResult;
      server_result._id = id;
      server_result._rev = rev;
      model.save();
      expect(model.get('_id')).toEqual(id);
      expect(model.get('_rev')).toEqual(rev);
      expect(model.get('_key')).toEqual(myKey);
      expect(model.get('edges')).toEqual(e);
      expect(model.get('vertices')).toEqual(v);
      expect(model.get('error')).toBeUndefined();
      expect(model.get('code')).toBeUndefined();
      expect(model.get('graph')).toBeUndefined();
    });

    it('should request /_api/graph/_key on delete', function () {
      model.save();
      ajaxVerify = function (opt) {
        expect(opt.url).toEqual('/_api/gharial/' + myKey);
        expect(opt.type).toEqual('DELETE');
      };
      model.destroy();
      expect($.ajax).toHaveBeenCalled();
    });

    it('should addEdgeDefinition', function () {
      ajaxVerify = function (opt) {
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('POST');
        expect(opt.url).toEqual('/_api/gharial/' + myKey + '/edge');
        expect(opt.data).toEqual('{"bla":"blub"}');
      };
      model.addEdgeDefinition({bla: 'blub'});
      expect($.ajax).toHaveBeenCalled();
    });

    it('should deleteEdgeDefinition', function () {
      ajaxVerify = function (opt) {
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('DELETE');
        expect(opt.url).toEqual('/_api/gharial/' + myKey + '/edge/ec');
      };
      model.deleteEdgeDefinition('ec');
      expect($.ajax).toHaveBeenCalled();
    });

    it('should modifyEdgeDefinition', function () {
      ajaxVerify = function (opt) {
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('PUT');
        expect(opt.url).toEqual('/_api/gharial/' + myKey + '/edge/ec');
        expect(opt.data).toEqual('{"bla":"blub","collection":"ec"}');
      };
      model.modifyEdgeDefinition({bla: 'blub', collection: 'ec'});
      expect($.ajax).toHaveBeenCalled();
    });

    it('should addVertexCollection', function () {
      ajaxVerify = function (opt) {
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('POST');
        expect(opt.url).toEqual('/_api/gharial/' + myKey + '/vertex');
        expect(opt.data).toEqual('{"collection":"vertexCollectionName"}');
      };
      model.addVertexCollection('vertexCollectionName');
      expect($.ajax).toHaveBeenCalled();
    });

    it('should deleteVertexCollection', function () {
      ajaxVerify = function (opt) {
        expect(opt.async).toEqual(false);
        expect(opt.type).toEqual('DELETE');
        expect(opt.url).toEqual('/_api/gharial/' + myKey + '/vertex/ec');
      };
      model.deleteVertexCollection('ec');
      expect($.ajax).toHaveBeenCalled();
    });
  });
}());
