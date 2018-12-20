import React, { Component } from 'react';
import Overview from './views/shards/overview';
import axios from 'axios';
import jsoneditor from 'jsoneditor';
import * as d3 from 'd3';
//import d3 from 'd3';
import nvd3 from 'nvd3';

// import logo from './logo.svg';
import './App.css';

// old libraries
import $ from 'jquery';
import jQuery from 'jquery';
// import 'jquery-ui/ui/core';
// import 'jquery-ui/ui/widgets/dialog';

import Backbone from 'backbone';
import _ from 'underscore';
import Noty from 'noty';

// old css files
import '../../frontend/build/extra-minified.css';
import '../../frontend/build/style-minified.css';

// window._ = _;

function requireAll(context) {
  context.keys().forEach(context);
}

/**
 * `require` all backbone dependencies
 */

// duplicate, needs to be removed? see import above
window.jquery = window.$ = window.jQuery = require(
  '../../frontend/js/lib/jquery-2.1.0.min.js'
);
//window.$.noty = Noty;
//window.noty = Noty;
//require('../../frontend/js/lib/jquery.noty.packaged.min.js');
window.Joi = require('../../frontend/js/lib/joi-browser.min.js');
require('../../frontend/js/lib/select2.min.js');

window._ = require('../../frontend/js/lib/underscore-min.js');
window._ = _;
require('../../frontend/js/arango/templateEngine.js');
require('../../frontend/js/arango/arango.js');

// only set this for development
if (window.frontendConfig) {
  window.frontendConfig.basePath = "http://localhost:8529";
  window.frontendConfig.react = true;
}

require(
  '../../frontend/js/lib/jquery-ui-1.9.2.custom.min.js'
);
// require('../../frontend/js/lib/jquery.snippet.min.js');
require(
  '../../frontend/js/lib/bootstrap-min.js'
);

/*
window.Backbone = require(
  '../../frontend/js/lib/backbone-min.js'
);*/

// Collect all Backbone.js relateds
require('../../frontend/js/routers/router.js');
require('../../frontend/js/routers/versionCheck.js');
require('../../frontend/js/routers/startApp.js');

requireAll(require.context(
  '../../frontend/js/views/'
));

requireAll(require.context(
  '../../frontend/js/models/'
));

requireAll(require.context(
  '../../frontend/js/collections/'
));

// Third Party Libraries
require('../../frontend/js/lib/tippy.js');
require('../../frontend/js/lib/bootstrap-pagination.min.js');
window.numeral = require('../../frontend/js/lib/numeral.min.js'); // TODO 
// require('../../frontend/js/lib/jsoneditor-min.js');
//require('../../frontend/build/libs.js')
window.JSONEditor = jsoneditor;
window.d3 = d3;
// require('../../frontend/js/lib/nv.d3.min.js');

window.prettyBytes = require('../../frontend/js/lib/pretty-bytes.js');

class App extends Component {
  // <Overview />
  render() {
    return (
      <div className="App">
        <h2>Test </h2>
        
      </div>
    );
  }
}

export default App;
