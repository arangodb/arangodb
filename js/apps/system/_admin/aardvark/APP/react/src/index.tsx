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
taskRepeater.registerTask("healthCheck", new CheckHealthTask());