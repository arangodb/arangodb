var jsunity = require("jsunity");

function FormatMiddlewareSpec () {
  var Middleware, middleware, request, response, options, next;

  return {
    setUp: function () {
      Middleware = require('org/arangodb/foxx/format_middleware').FormatMiddleware;
      request = {};
      response = {};
      options = {};
      next = function () {};
    },

    testChangesTheURLAccordingly: function () {
      request = { path: "test/1.json", headers: {} };
      middleware = new Middleware(["json"]);
      middleware(request, response, options, next);
      assertEqual(request.path, "test/1");
    },

    testRefusesFormatsThatHaveNotBeenAllowed: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      middleware = new Middleware(["json"]);
      request = { path: "test/1.html", headers: {} };
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual("<" + response.body + ">", "<Error: Format 'html' is not allowed.>");
      assertFalse(nextCalled);
    },

    testRefuseContradictingURLAndResponseType: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1.json", headers: {"accept": "text/html"} };
      middleware = new Middleware(["json"]);
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual("<" + response.body + ">", "<Error: Contradiction between Accept Header and URL.>");
      assertFalse(nextCalled);
    },

    testRefuseMissingBothURLAndResponseTypeWhenNoDefaultWasGiven: function () {
      var nextCalled = false,
        next = function () { nextCalled = true; };
      request = { path: "test/1", headers: {} };
      middleware = new Middleware(["json"]);
      middleware(request, response, options, next);
      assertEqual(response.responseCode, 406);
      assertEqual("<" + response.body + ">", "<Error: Format 'undefined' is not allowed.>");
      assertFalse(nextCalled);
    },

    testFallBackToDefaultWhenMissingBothURLAndResponseType: function () {
      request = { path: "test/1", headers: {} };
      middleware = new Middleware(["json"], "json");
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    // JSON
    testSettingTheFormatAttributeAndResponseTypeForJsonViaURL: function () {
      request = { path: "test/1.json", headers: {} };
      middleware = new Middleware(["json"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    testSettingTheFormatAttributeAndResponseTypeForJsonViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "application/json"} };
      middleware = new Middleware(["json"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "json");
      assertEqual(response.contentType, "application/json");
    },

    // HTML
    testSettingTheFormatAttributeAndResponseTypeForHtmlViaURL: function () {
      request = { path: "test/1.html", headers: {} };
      middleware = new Middleware(["html"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "html");
      assertEqual(response.contentType, "text/html");
    },

    testSettingTheFormatAttributeAndResponseTypeForHtmlViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "text/html"} };
      middleware = new Middleware(["html"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "html");
      assertEqual(response.contentType, "text/html");
    },

    // TXT
    testSettingTheFormatAttributeAndResponseTypeForTxtViaURL: function () {
      request = { path: "test/1.txt", headers: {} };
      middleware = new Middleware(["txt"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "txt");
      assertEqual(response.contentType, "text/plain");
    },

    testSettingTheFormatAttributeAndResponseTypeForTxtViaAcceptHeader: function () {
      request = { path: "test/1", headers: {"accept": "text/plain"} };
      middleware = new Middleware(["txt"]);
      middleware(request, response, options, next);
      assertEqual(request.format, "txt");
      assertEqual(response.contentType, "text/plain");
    }
  };
}

jsunity.run(FormatMiddlewareSpec);

return jsunity.done();
