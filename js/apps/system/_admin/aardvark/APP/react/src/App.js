import React, { Component } from 'react';
import ReactDOM from 'react-dom';
import Overview from './views/shards/overview';
import jsoneditor from 'jsoneditor';
import * as d3 from 'd3';
//import d3 from 'd3';
import nvd3 from 'nvd3';
import * as fs from 'fs';

// import logo from './logo.svg';
import './App.css';

// import new react views

import './views/shards/ShardsReactView';

// old libraries
// import $ from 'jquery';
import jQuery from 'jquery';

import Backbone from 'backbone';
import _ from 'underscore';
import Sigma from 'sigma';

// highlight.js
import hljs from 'highlight.js/lib/highlight';
import json from 'highlight.js/lib/languages/json';

// import old based css files
import '../../frontend/css/pure-min.css';
import '../../frontend/ttf/arangofont/style.css';
import '../../frontend/css/bootstrap.css';
import '../../frontend/css/jquery.contextmenu.css';
import '../../frontend/css/select2.css';
// import '../../frontend/css/select2-bootstrap.css';
import '../../frontend/css/highlightjs.css';
import '../../frontend/css/jsoneditor.css';
import '../../frontend/css/tippy.css';
import '../../frontend/css/dygraph.css';
import '../../frontend/css/leaflet.css';
import '../../frontend/css/nv.d3.css';
import '../../frontend/css/grids-responsive-min.css';

// import sass files
import '../../frontend/scss/style.scss';
import 'highlight.js/styles/github.css';

window.JST = {};

// import sass files
function requireAll(context) {
  context.keys().forEach(context);
  _.each(context.keys(), function(key) {
    // detect html, later move to ejs back again
    if (key.substring(key.length - 5, key.length) === '.html') {
      let filename = key.substring(2, key.length);
      let name = key.substring(2, key.length - 5);
      window.JST['templates/' + name] = _.template(require('../../frontend/js/templates/' + filename));
    }
  });
}

// templates ejs
requireAll(require.context(
  '../../frontend/js/templates/'
));

/**
 * `require` all backbone dependencies
 */

hljs.registerLanguage('json', json);
window.hljs = hljs;

window.ReactDOM = ReactDOM;
window.Joi = require('../../frontend/js/lib/joi-browser.min.js');
window.jQuery = window.$ = jQuery;

require('../../frontend/js/lib/select2.min.js');
window._ = _;
require('../../frontend/js/arango/templateEngine.js');
require('../../frontend/js/arango/arango.js');

// only set this for development
const env = process.env.NODE_ENV
if (window.frontendConfig && env === 'development') {
  window.frontendConfig.basePath = process.env.REACT_APP_ARANGODB_HOST;
  window.frontendConfig.react = true;
}

require('../../frontend/js/lib/jquery-ui-1.9.2.custom.min.js');
require('../../frontend/js/lib/bootstrap-min.js');

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
window.tippy = require('tippy.js');
require('../../frontend/js/lib/bootstrap-pagination.min.js');
window.numeral = require('../../frontend/js/lib/numeral.min.js'); // TODO 
window.JSONEditor = jsoneditor;
// ace 
window.define = window.ace.define;
window.aqltemplates = require('../../frontend/aqltemplates.json');

window.d3 = d3;
require('../../frontend/js/lib/leaflet.js')
require('../../frontend/js/lib/tile.stamen.js')

// require('../../frontend/js/lib/nv.d3.min.js');

window.prettyBytes = require('../../frontend/js/lib/pretty-bytes.js');
window.Dygraph = require('../../frontend/js/lib/dygraph-combined.min.js');
require('../../frontend/js/config/dygraphConfig.js');
window.moment = require('../../frontend/js/lib/moment.min.js');

// sigma
window.sigma = Sigma;

// import additional sigma plugins
import('sigma/build/plugins/sigma.layout.forceAtlas2.min'); // workaround to work with webpack

// additional sigma plugins
import('../../frontend/js/lib/sigma.canvas.edges.autoCurve.js');
import('../../frontend/js/lib/sigma.canvas.edges.curve.js');
import('../../frontend/js/lib/sigma.canvas.edges.dashed.js');
import('../../frontend/js/lib/sigma.canvas.edges.dotted.js');
import('../../frontend/js/lib/sigma.canvas.edges.labels.curve.js');
import('../../frontend/js/lib/sigma.canvas.edges.labels.curvedArrow.js');
import('../../frontend/js/lib/sigma.canvas.edges.labels.def.js');
import('../../frontend/js/lib/sigma.canvas.edges.tapered.js');
import('../../frontend/js/lib/sigma.exporters.image.js');
import('../../frontend/js/lib/sigma.layout.fruchtermanReingold.js');
import('../../frontend/js/lib/sigma.layout.noverlap.js');
import('../../frontend/js/lib/sigma.plugins.animate.js');
import('../../frontend/js/lib/sigma.plugins.dragNodes.js');
import('../../frontend/js/lib/sigma.plugins.filter.js');
import('../../frontend/js/lib/sigma.plugins.fullScreen.js');
import('../../frontend/js/lib/sigma.plugins.lasso.js');
import('../../frontend/js/lib/sigma.renderers.halo.js');
import('../../frontend/js/lib/jquery.csv.min.js');

require('../../frontend/js/lib/wheelnav.slicePath.js');
require('../../frontend/js/lib/wheelnav.min.js');
window.Raphael = require('../../frontend/js/lib/raphael.min.js');
require('../../frontend/js/lib/raphael.icons.min.js');

//require('../../frontend/src/ace.js');
//require('../../frontend/src/theme-textmate.js');
//require('../../frontend/src/mode-json.js');
//require('../../frontend/src/mode-aql.js');

class App extends Component {
  // <Overview />
  render() {
    return (
      <div className="App"></div>
    );
  }
}

export default App;
