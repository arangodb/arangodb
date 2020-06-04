import "./index.css";
import React from "react";
import ReactDOM from "react-dom";
import { Provider } from 'react-redux';
import App from "./App";
import store from "./configureStore";
import { checkHealth } from "./store/cluster/actions";
import { taskRepeater, Task } from "./actions/taskRepeater";

ReactDOM.render(<Provider store={store}>
  <App />
</Provider>, document.getElementById("root"));

class CheckHealthTask implements Task {
  execute() {
    store.dispatch(checkHealth());
  }
}

// TODO: Current workaround, we need frontendConfig state to be within redux
// and overall be accessible.

let myWindow = window as any;

if (myWindow.frontendConfig.isCluster) {
  taskRepeater.registerTask("healthCheck", new CheckHealthTask());
}