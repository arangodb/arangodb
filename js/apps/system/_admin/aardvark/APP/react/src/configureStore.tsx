import { Action, createStore, Store } from 'redux';
import { ApplicationState, initialState, middlewares, reducers } from './store';

function configureStore (): Store<ApplicationState> {
  return createStore<ApplicationState, Action, {}, {}>(reducers, initialState, middlewares);
}

const store = configureStore();
export default store;
