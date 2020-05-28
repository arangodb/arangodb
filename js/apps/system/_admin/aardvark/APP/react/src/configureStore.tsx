import { createStore, Action, Store } from 'redux';
import { ApplicationState, reducers, initialState, middlewares } from './store';

function configureStore(): Store<ApplicationState> {
  return createStore<ApplicationState, Action<any>, {}, {}>(reducers, initialState, middlewares);
}

const store = configureStore();
export default store;