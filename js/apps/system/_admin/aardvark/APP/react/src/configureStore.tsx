import { Action, createStore, Store } from 'redux';
import { Task, taskRepeater } from './actions/taskRepeater';
import { ApplicationState, initialState, middlewares, reducers } from './store';
import { checkHealth } from './store/cluster/actions';

function configureStore(): Store<ApplicationState> {
  return createStore<ApplicationState, Action, {}, {}>(reducers, initialState, middlewares);
}

const store = configureStore();
export default store;

class CheckHealthTask implements Task {
  execute() {
    store.dispatch(checkHealth());
  }
}

// TODO: Current workaround, we need frontendConfig state to be within redux
// and overall be accessible.

if (window.frontendConfig.isCluster) {
  taskRepeater.registerTask("healthCheck", new CheckHealthTask());
}
