import { Action, Middleware, MiddlewareAPI } from "redux";
import { Dispatch } from "react";


const middleware : Middleware = (_: MiddlewareAPI) => (next: Dispatch<Action>) => (action : Action) => {
  /* console.log(Date.now(), "Redux Log:", action); */
  return next(action);
};

export default middleware;
