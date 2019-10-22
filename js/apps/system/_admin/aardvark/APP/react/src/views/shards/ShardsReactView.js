import React, { Component } from 'react';
import ReactDOM from 'react-dom';
import Overview from './overview';
import jQuery from 'jquery';

import { Provider } from 'react-redux';
import store from "../../configureStore";

class ShardsReactView {
  render () {
    ReactDOM.render(<Provider store={store}>
        <Overview />
      </Provider>, document.getElementById("content"));
  }
};

window.ShardsReactView = new ShardsReactView();