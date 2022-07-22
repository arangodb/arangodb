/* global */

import React from 'react';
import 'antd/dist/antd.css';
import G6JsQueryGraph from './G6JsQueryGraph';

const G6QueryView = (result) => {
  return <div>
    <G6JsQueryGraph result={result} />
  </div>;
};

window.G6QueryView = G6QueryView;
