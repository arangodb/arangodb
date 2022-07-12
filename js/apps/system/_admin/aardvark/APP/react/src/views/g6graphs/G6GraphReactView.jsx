/* global */

import React from 'react';
import './graph.css';
import G6JsGraph from './G6JsGraph';

const G6GraphReactView = () => {
  return <div className="graphReactViewContainer">
    <G6JsGraph />
  </div>;
};

window.G6GraphReactView = G6GraphReactView;
