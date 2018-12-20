import React from 'react';
import ReactDOM from 'react-dom';
import { Provider } from 'react-redux';
import store from './actions/store';
import './index.css';
import App from './App';
import * as serviceWorker from './serviceWorker';
import { SHARDS_OVERVIEW_FETCH } from './actions/actions';

ReactDOM.render(<Provider store={store}>
  <App />
</Provider>, document.getElementById('root'));

store.dispatch({type: SHARDS_OVERVIEW_FETCH});
// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: http://bit.ly/CRA-PWA
serviceWorker.unregister();
