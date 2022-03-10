import React from 'react';

import { Provider } from 'react-redux';
import store from '../../configureStore';
import Overview from './overview';

class ShardsReactView {
  render () {
    return <Provider store={store}>
      <Overview/>
    </Provider>;
  }
}

window.ShardsReactView = ShardsReactView;
