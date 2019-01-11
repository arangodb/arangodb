import React, { Component } from 'react';
import ReactDOM from 'react-dom';
import Overview from './views/shards/overview';
import jsoneditor from 'jsoneditor';
import * as d3 from 'd3';
//import d3 from 'd3';
import nvd3 from 'nvd3';

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

// import old based css files
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
import '../../frontend/css/pure-min.css';
import '../../frontend/css/grids-responsive-min.css';

// import sass files
import '../../frontend/scss/style.scss';
import('sigma/build/plugins/sigma.layout.forceAtlas2.min'); // TODO maybe also import other plugins like this

// import sass files
function requireAll(context) {
  context.keys().forEach(context);
}

// templates ejs
requireAll(require.context(
  '../../frontend/js/templates/'
));

/**
 * `require` all backbone dependencies
 */

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
  window.frontendConfig.basePath = "http://localhost:8529";
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
require('../../frontend/js/lib/tippy.js');
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
//window.sigma = require('../../frontend/js/lib/sigma.min.js');

// window.sigma = Sigma;
window.sigma = Sigma;
require('../../frontend/js/lib/sigma.canvas.edges.autoCurve.js');
require('../../frontend/js/lib/sigma.canvas.edges.curve.js');
require('../../frontend/js/lib/sigma.canvas.edges.dashed.js');
require('../../frontend/js/lib/sigma.canvas.edges.dotted.js');
require('../../frontend/js/lib/sigma.canvas.edges.labels.curve.js');
require('../../frontend/js/lib/sigma.canvas.edges.labels.curvedArrow.js');
require('../../frontend/js/lib/sigma.canvas.edges.labels.def.js');
require('../../frontend/js/lib/sigma.canvas.edges.tapered.js');
require('../../frontend/js/lib/sigma.exporters.image.js');
require('../../frontend/js/lib/sigma.layout.fruchtermanReingold.js');
require('../../frontend/js/lib/sigma.layout.noverlap.js');
require('../../frontend/js/lib/sigma.plugins.animate.js');
require('../../frontend/js/lib/sigma.plugins.dragNodes.js');
require('../../frontend/js/lib/sigma.plugins.filter.js');
require('../../frontend/js/lib/sigma.plugins.fullScreen.js');
require('../../frontend/js/lib/sigma.plugins.lasso.js');
require('../../frontend/js/lib/sigma.renderers.halo.js');
require('../../frontend/js/lib/jquery.csv.min.js');
//require('../../frontend/js/lib/worker.js');
//require('../../frontend/js/lib/supervisor.js');

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
      <div className="App">
        <h2>Test </h2>
        
      </div>
    );
  }
}

export default App;
