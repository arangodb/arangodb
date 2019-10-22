import { Action } from 'redux';

export type ShardDistributionResponse = {
  [collection: string] : {
    Plan: any,
    Current: any
  } 
};

export type MoveShardInfo = {
  collection: string,
  shard: string,
  fromServer: string,
  toServer: string
};

export interface ClusterState {
  readonly shortToServer: Map<string, string>,
  readonly serverToShort: Map<string, string>,
  readonly shardDistribution : ShardDistributionResponse,
  readonly selected : string
};

export interface HealthInfo {
  [key: string] : {
    ShortName? : string
  }
}

export interface CheckHealth extends Action {
  type: '@@cluster/checkHealth'
};

export interface CheckHealthSuccess extends Action {
  type: '@@cluster/checkHealthSuccess',
  payload: HealthInfo
};

export interface FetchShardDetails extends Action {
  type: '@@cluster/fetchShardDetails',
  payload: string
};

export interface FetchShardDetailsSuccess extends Action {
  type: '@@cluster/fetchShardDetailsSuccess',
  payload: ShardDistributionResponse
};

export interface FetchShardOverview extends Action {
  type: '@@cluster/fetchShardOverview'
};

export interface FetchShardOverviewSuccess extends Action {
  type: '@@cluster/fetchShardOverviewSuccess',
  payload: ShardDistributionResponse
};

export interface UnselectShardDetails extends Action {
  type: '@@cluster/unselectShardDetails'
};

export interface MoveShard extends Action {
  type: '@@cluster/moveShard',
  payload: MoveShardInfo
};

export interface RebalanceShards extends Action {
  type: '@@cluster/rebalanceShards'
};

export type ClusterActions = CheckHealth | CheckHealthSuccess |
                             FetchShardDetails | FetchShardDetailsSuccess |
                             FetchShardOverview | FetchShardOverviewSuccess |
                             MoveShard | RebalanceShards |
                             UnselectShardDetails;