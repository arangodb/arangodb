/* global describe, it, beforeEach */
'use strict';
require('chai').config.truncateThreshold = 0;
const joi = require('joi');
const expect = require('chai').expect;
const Service = require('@arangodb/foxx/service');
const createRouter = require('@arangodb/foxx/router');

describe('Foxx Swagger', function () {
  let service;
  beforeEach(function () {
    service = createService();
  });

  it('adds all the basic metadata', function () {
    service.buildRoutes();
    expect(service.docs).to.eql({
      swagger: '2.0',
      basePath: `/_db/_system${service.mount}`,
      paths: {},
      info: {
        title: service.manifest.name,
        description: service.manifest.description,
        version: service.manifest.version,
        license: {
          name: service.manifest.license
        }
      }
    });
  });

  describe('Route Info', function () {
    it('formats path param names', function () {
      service.router.get('/:x', noop());
      service.buildRoutes();
      expect(service.docs.paths).to.have.a.property('/{x}')
        .with.a.property('get');
    });

    it('disambiguates duplicate path param names', function () {
      const child = createRouter();
      child.get('/:x', noop());
      service.router.use('/:x', child);
      service.buildRoutes();
      expect(service.docs.paths).to.have.a.property('/{x}/{x2}')
        .with.a.property('get');
    });

    it('supports multiple methods on the same path', function () {
      service.router.get('/a', noop()).description('x');
      service.router.post('/a', noop()).description('y');
      service.buildRoutes();
      expect(service.docs.paths).to.have.a.property('/a')
        .with.a.deep.property('get.description', 'x');
      expect(service.docs.paths).to.have.a.property('/a')
        .with.a.deep.property('post.description', 'y');
    });

    describe('"deprecated"', function () {
      it('is omitted by default', function () {
        service.router.get('/hello', noop());
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.not.a.deep.property('get.deprecated');
      });

      it('is set to true if the route is marked deprecated', function () {
        service.router.get('/hello', noop()).deprecated();
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.deprecated')
          .that.is.equal(true);
      });

      it('is set to true if a parent is marked deprecated', function () {
        const child = createRouter();
        child.get('/hello', noop()).deprecated(false);
        service.router.use(child).deprecated();
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.deprecated')
          .that.is.equal(true);
      });

      it('is omitted if the route is marked un-deprecated', function () {
        service.router.get('/hello', noop()).deprecated(false);
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.not.a.deep.property('get.deprecated');
      });
    });

    ['description', 'summary'].forEach(function (field) {
      describe(`"${field}"`, function () {
        it('is omitted by default', function () {
          service.router.get('/hello', noop());
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.not.a.deep.property(`get.${field}`);
        });

        it(`is set to the ${field} if provided`, function () {
          const str = 'Hello World!';
          service.router.get('/hello', noop())[field](str);
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.a.deep.property(`get.${field}`)
            .that.is.equal(str);
        });

        it(`is omitted if the ${field} is empty`, function () {
          service.router.get('/hello', noop())[field]('');
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.not.a.deep.property(`get.${field}`);
        });
      });
    });

    describe('"consumes"', function () {
      ['patch', 'post', 'put'].forEach(function (method) {
        it(`is omitted for ${method.toUpperCase()} by default`, function () {
          service.router[method]('/hello', noop());
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.not.a.deep.property(`${method}.consumes`);
        });
      });

      ['get', 'head', 'delete'].forEach(function (method) {
        it(`is empty for ${method.toUpperCase()} by default`, function () {
          service.router[method]('/hello', noop());
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.a.deep.property(`${method}.consumes`)
            .that.is.eql([]);
        });
      });

      it('is set to the body mime type', function () {
        service.router.post('/hello', noop()).body(['text/plain']);
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('post.consumes')
          .that.is.eql(['text/plain']);
      });

      ['get', 'head', 'delete'].forEach(function (method) {
        it(`is set for ${method.toUpperCase()} if explicitly defined`, function () {
          service.router[method]('/hello', noop()).body(['text/plain']);
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.a.deep.property(`${method}.consumes`)
            .that.is.eql(['text/plain']);
        });
      });

      ['patch', 'post', 'put'].forEach(function (method) {
        it(`is empty for ${method.toUpperCase()} if explicitly set to null`, function () {
          service.router[method]('/hello', noop()).body(null);
          service.buildRoutes();
          expect(service.docs.paths).to.have.a.property('/hello')
            .with.a.deep.property(`${method}.consumes`)
            .that.is.eql([]);
        });
      });
    });

    describe('"produces"', function () {
      it('defaults to JSON', function () {
        service.router.get('/hello', noop());
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.produces')
          .that.is.eql(['application/json']);
      });

      it('includes explicit response types', function () {
        service.router.get('/hello', noop()).response(['text/plain']);
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.produces')
          .that.is.eql(['text/plain', 'application/json']);
      });

      it('includes non-200 response types', function () {
        service.router.get('/hello', noop())
          .response(['text/plain'])
          .response(499, ['text/html']);
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.produces')
          .that.is.eql(['text/plain', 'text/html', 'application/json']);
      });
    });

    describe('"responses"', function () {
      it('provides a 500 response by default', function () {
        service.router.get('/hello', noop());
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.responses.500')
          .that.has.a.property('description', 'Default error response.');
      });

      it('does not provide any other default responses', function () {
        service.router.get('/hello', noop());
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.responses')
          .that.has.all.keys('500');
      });

      it('includes explicit responses', function () {
        service.router.get('/hello', noop())
          .response(200, 'Some response')
          .response(400, 'Some error');
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.responses.200')
          .that.has.a.property('description', 'Some response');
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.responses.400')
          .that.has.a.property('description', 'Some error');
      });

      it('includes explicit schemas', function () {
        service.router.get('/hello', noop())
          .response(200, joi.object());
        service.buildRoutes();
        expect(service.docs.paths).to.have.a.property('/hello')
          .with.a.deep.property('get.responses.200')
          .that.has.a.property('schema')
          .that.is.eql({
          type: 'object',
          properties: {},
          additionalProperties: false
        });
      });
    });

    describe('"parameters"', function () {
      describe('in:body', function () {
        it('TODO');
      });

      describe('in:path', function () {
        it('TODO');
      });

      describe('in:query', function () {
        it('TODO');
      });

      describe('in:header', function () {
        it('TODO');
      });
    });
  });
});

function createService () {
  return new Service({
    path: '/tmp/$dummy$',
    mount: '/__dummy__',
    manifest: {
      name: 'DUMMY APP',
      version: '1.0.0',
      license: 'The Unlicense',
      description: 'Dummy app for Foxx.',
      main: '$dummy$',
      engines: {
        arangodb: '^3.0.0'
      }
    },
    options: {}
  });
}

function noop () {
  return noop;
}
