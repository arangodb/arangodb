/* global */

import React from 'react';
import './graph.css';
import VisJsGraph from './VisJsGraph';

const VisGraphReactView = () => {
  return <div className="graphReactViewContainer">
    <VisJsGraph />
  </div>;
};

window.VisGraphReactView = VisGraphReactView;
