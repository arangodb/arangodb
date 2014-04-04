// Karma configuration
// Generated on Thu Jul 04 2013 11:39:34 GMT+0200 (CEST)

filesJSON = require("./files.json");

module.exports = function(karma) {

  karma.set({

    // base path, that will be used to resolve files and exclude
    basePath: '../../',


    // frameworks to use
    //frameworks: ['jasmine', 'junit-reporter'],
    frameworks: ['jasmine'],


    // list of files / patterns to load in the browser
    files: filesJSON.files,

    // list of files to exclude
    exclude: [
    ],

    preprocessors: {
      'test/karma/files.json': ['html2js'],
      'clusterFrontend/js/**/**.js': ['coverage'],
      'frontend/js/arango/**.js': ['coverage'],
      'frontend/js/bootstrap/**.js': ['coverage'],
      'frontend/js/client/**.js': ['coverage'],
      'frontend/js/collections/**.js': ['coverage'],
      'frontend/js/config/**.js': ['coverage'],
      'frontend/js/graphViewer/**.js': ['coverage'],
      'frontend/js/models/**.js': ['coverage'],
      'frontend/js/modules/**.js': ['coverage'],
      'frontend/js/routers/**.js': ['coverage'],
      'frontend/js/shell/**.js': ['coverage'],
      'frontend/js/templates/**.js': ['coverage'],
      'frontend/js/views/**.js': ['coverage']
    },

    // test results reporter to use
    // possible values: 'dots', 'progress', 'junit', 'growl', 'coverage'
    reporters: ['dots', 'coverage'],

    coverageReporter: {
      type : 'html',
      dir : 'coverage/'
    },

    // web server port
    port: 9876,


    // cli runner port
    runnerPort: 9100,

    // enable / disable colors in the output (reporters and logs)
    colors: false,


    // level of logging
    // possible values: karma.LOG_DISABLE || karma.LOG_ERROR || karma.LOG_WARN || karma.LOG_INFO || karma.LOG_DEBUG
    logLevel: karma.LOG_INFO,


    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: false,


    // Start these browsers, currently available:
    // - Chrome
    // - ChromeCanary
    // - Firefox
    // - Opera
    // - Safari (only Mac)
    // - PhantomJS
    // - IE (only Windows)
    browsers: ["PhantomJS"],

    // If browser does not capture in given timeout [ms], kill it
    captureTimeout: 60000,


    // Continuous Integration mode
    // if true, it capture browsers, run tests and exit
    singleRun: true
  });
};
