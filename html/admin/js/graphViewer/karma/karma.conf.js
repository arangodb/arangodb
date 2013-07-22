// Karma configuration
// Generated on Thu Jul 04 2013 11:39:34 GMT+0200 (CEST)

module.exports = function(karma) {
  karma.set({

    // base path, that will be used to resolve files and exclude
    basePath: '../jasmine_test/',


    // frameworks to use
    frameworks: ['jasmine'],


    // list of files / patterns to load in the browser
    files: [
    'lib/jasmine-1.3.1/jasmine-html.js',
    'lib/jslint.js',
    '../../lib/d3.v3.min.js',
    '../../lib/d3.fisheye.js',
    '../../lib/underscore.js',
    '../../lib/jquery-1.8.3.js',
    '../../lib/bootstrap.js',
    '../../lib/jquery.livequery.js',
    
    // Mocks
      'helper/eventHelper.js',
      'helper/objectsHelper.js',
      'helper/mocks.js',
      'helper/commMock.js',
      'helper/uiMatchers.js',

    // Core Modules
      '../graphViewer.js',
      '../graph/domObserverFactory.js',
      '../graph/colourMapper.js',
      '../graph/communityNode.js',
      '../graph/webWorkerWrapper.js',
      '../graph/nodeShaper.js',
      '../graph/abstractAdapter.js',
      '../graph/jsonAdapter.js',
      '../graph/arangoAdapter.js',
      '../graph/foxxAdapter.js',
      '../graph/previewAdapter.js',
      '../graph/edgeShaper.js',
      '../graph/forceLayouter.js',
      '../graph/eventDispatcher.js',
      '../graph/eventLibrary.js',
      '../graph/zoomManager.js',
      '../graph/nodeReducer.js',
      '../graph/modularityJoiner.js',
    // UI Modules
      '../ui/modalDialogHelper.js',
      '../ui/nodeShaperControls.js',
      '../ui/edgeShaperControls.js',
      '../ui/arangoAdapterControls.js',
      '../ui/layouterControls.js',
      '../ui/uiComponentsHelper.js',
      '../ui/eventDispatcherControls.js',
      '../ui/graphViewerUI.js',
      '../ui/graphViewerWidget.js',
      '../ui/graphViewerPreview.js',
  
    // Specs
    
      'specColourMapper/colourMapperSpec.js',
      'specWindowObjects/domObserverFactorySpec.js',
      'specCommunityNode/communityNodeSpec.js',
      'specAdapter/interfaceSpec.js',
      'specAdapter/abstractAdapterSpec.js',
      'specAdapter/jsonAdapterSpec.js',
      'specAdapter/arangoAdapterSpec.js',
      'specAdapter/foxxAdapterSpec.js',
      'specAdapter/previewAdapterSpec.js',
      'specAdapter/arangoAdapterUISpec.js',
      'specNodeShaper/nodeShaperSpec.js',
      'specNodeShaper/nodeShaperUISpec.js',
      'specEdgeShaper/edgeShaperSpec.js',
      'specEdgeShaper/edgeShaperUISpec.js',
      'specForceLayouter/forceLayouterSpec.js',
      'specForceLayouter/forceLayouterUISpec.js',
      'specEvents/eventLibrarySpec.js',
      'specEvents/eventDispatcherSpec.js',
      'specEvents/eventDispatcherUISpec.js',
      'specZoomManager/zoomManagerSpec.js',
      'specGraphViewer/graphViewerSpec.js',
      'specGraphViewer/graphViewerUISpec.js',
      'specGraphViewer/graphViewerWidgetSpec.js',
      'specGraphViewer/graphViewerPreviewSpec.js',
      'specNodeReducer/nodeReducerSpec.js',
      'specNodeReducer/modularityJoinerSpec.js',
      'specWindowObjects/workerWrapperSpec.js',
      'specJSLint/jsLintSpec.js'
    ],


    // list of files to exclude
    exclude: [
      
    ],


    // test results reporter to use
    // possible values: 'dots', 'progress', 'junit', 'growl', 'coverage'
    reporters: ['progress'],


    // web server port
    port: 9876,


    // cli runner port
    runnerPort: 9100,


    // enable / disable colors in the output (reporters and logs)
    colors: true,


    // level of logging
    // possible values: karma.LOG_DISABLE || karma.LOG_ERROR || karma.LOG_WARN || karma.LOG_INFO || karma.LOG_DEBUG
    logLevel: karma.LOG_INFO,


    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: true,


    // Start these browsers, currently available:
    // - Chrome
    // - ChromeCanary
    // - Firefox
    // - Opera
    // - Safari (only Mac)
    // - PhantomJS
    // - IE (only Windows)
    browsers: [],


    // If browser does not capture in given timeout [ms], kill it
    captureTimeout: 60000,


    // Continuous Integration mode
    // if true, it capture browsers, run tests and exit
    singleRun: false
  });
};
