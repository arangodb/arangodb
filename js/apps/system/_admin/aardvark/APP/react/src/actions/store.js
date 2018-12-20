import { createStore, applyMiddleware } from "redux";
import reducer from "./reducer"
import loggingMiddleware from "../middlewares/logging"
import clusterMiddleware from "../middlewares/cluster"

const initialState = {
  shardDistribution: {}
};



const store = createStore(reducer,
  initialState,
  applyMiddleware(
    loggingMiddleware,
    clusterMiddleware
  )
);

export default store;
