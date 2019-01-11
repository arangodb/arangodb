import { combineReducers, Reducer, applyMiddleware } from 'redux';

import { ClusterState } from './cluster/types';
import clusterReducer, {initialState as clusterInitialState } from './cluster/reducer';
import clusterMiddleware from './cluster/middleware';
import loggingMiddleware from './logging/middleware';

export interface ApplicationState {
  readonly cluster: ClusterState;
}

export const reducers: Reducer<ApplicationState> = combineReducers<ApplicationState>({
  cluster: clusterReducer
});

export const initialState : ApplicationState = {
  cluster: clusterInitialState
};

export const middlewares = applyMiddleware(clusterMiddleware, loggingMiddleware);