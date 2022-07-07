/* global */

import React from 'react';
import 'antd/dist/antd.css';
import G6JsGraph from './G6JsGraph';

const G6QueryView = () => {
  return <div>
    <G6JsGraph />
  </div>;
};

window.G6QueryView = G6QueryView;
