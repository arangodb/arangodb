var jsunity = require("jsunity"),
  arangodb = require("org/arangodb"),
  db = arangodb.db;

function TemplateMiddlewareSpec () {
  var TemplateMiddleware, templateMiddleware, templateCollection, request, response, options, next, error;

  return {
    setUp: function () {
      request = {};
      response = {};
      options = {};
      next = function () {};
      TemplateMiddleware = require("org/arangodb/foxx/template_middleware").TemplateMiddleware;
      db._drop("templateTest");
      templateCollection = db._create("templateTest");
    },

    testRenderingATemplateWhenInitializedWithACollection: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hello moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testRenderingATemplateWhenInitializedWithACollectionNameOfAnExistingCollection: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware("templateTest");
      templateMiddleware(request, response, options, next);

      response.render("simple/path", { username: "moonglum" });
      assertEqual(response.body, "hello moonglum");
      assertEqual(response.contentType, "text/plain");
    },

    testShouldCreateCollectionIfNotFound: function () {
      db._drop("templateTest");
      templateMiddleware = new TemplateMiddleware("templateTest");
      templateMiddleware(request, response, options, next);

      assertNotNull(db._collection("templateTest"));
    },

    testRenderingATemplateWithAnUnknownTemplateEngine: function () {
      templateCollection.save({
        path: "simple/path",
        content: "hello <%= username %>",
        contentType: "text/plain",
        templateLanguage: "pirateEngine"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Unknown template language 'pirateEngine'"));
    },

    testRenderingATemplateWithAnNotExistingTemplate: function () {
      templateMiddleware = new TemplateMiddleware(templateCollection);
      templateMiddleware(request, response, options, next);

      try {
        response.render("simple/path", { username: "moonglum" });
      } catch(e) {
        error = e;
      }

      assertEqual(error, new Error("Template 'simple/path' does not exist"));
    },

    testHelpers: function () {
      var a = false;

      templateCollection.save({
        path: "simple/path",
        content: "hello <%= testHelper() %>",
        contentType: "text/plain",
        templateLanguage: "underscore"
      });

      templateMiddleware = new TemplateMiddleware(templateCollection, {
        testHelper: function() { a = true; }
      });
      templateMiddleware(request, response, options, next);

      assertFalse(a);
      response.render("simple/path", {});
      assertTrue(a);
    }
  };
}

jsunity.run(TemplateMiddlewareSpec);

return jsunity.done();
