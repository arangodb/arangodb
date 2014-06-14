var Reporter,
  _ = require('underscore'),
  log = require('console').log,
  inspect = require('internal').inspect,
  p = function (x) { log(inspect(x)); };

Reporter = function () {
};

_.extend(Reporter.prototype, {
  jasmineStarted: function(options) {
    p(options);
    this.totalSpecsDefined = options.totalSpecsDefined || 0;
    log('Jasmine booting up, Number of Specs: %s', this.totalSpecsDefined);
  },

  jasmineDone: function() {
    log('Jasmine done');
  },

  suiteStarted: function(result) {
    log('Started Suite "%s"', result.description);
    p(result);
  },

  suiteDone: function(result) {
    log('Done with Suite "%s"', result.description);
    p(result);
  },

  specStarted: function(result) {
    log('Started Spec "%s"', result.description);
    p(result);
  },

  specDone: function(result) {
    log('Done with Spec "%s"', result.description);
    p(result);
  }
});

exports.Reporter = Reporter;
