/* global */

import React from 'react';
import './graph.css';
import VisJsGraph from './VisJsGraph';
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";

const VisGraphReactView = () => {
  useDisableNavBar();
  useGlobalStyleReset();
  
  return <div className="graphReactViewContainer">
    <VisJsGraph />
  </div>;
};

window.VisGraphReactView = VisGraphReactView;
