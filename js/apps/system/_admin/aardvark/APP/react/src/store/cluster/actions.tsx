import { ActionCreator } from "redux";
import { CheckHealth, CheckHealthSuccess, FetchShardDetails, FetchShardDetailsSuccess, HealthInfo, ShardDistributionResponse, UnselectShardDetails, RebalanceShards, MoveShard, MoveShardInfo, FetchShardOverview} from "./types";

export const checkHealth: ActionCreator<CheckHealth> = () => ({
  type: '@@cluster/checkHealth'
});

export const checkHealthSuccess: ActionCreator<CheckHealthSuccess> = (info: HealthInfo) => ({
  type: '@@cluster/checkHealthSuccess',
  payload: info
});

export const fetchShardDetails: ActionCreator<FetchShardDetails> = (name: string) => ({
  type: '@@cluster/fetchShardDetails',
  payload: name
});

export const fetchShardDetailsSuccess: ActionCreator<FetchShardDetailsSuccess> = (info: ShardDistributionResponse) => ({
  type: '@@cluster/fetchShardDetailsSuccess',
  payload: info
});

export const unselectShardDetails: ActionCreator<UnselectShardDetails> = () => ({
  type: '@@cluster/unselectShardDetails'
});

export const fetchShardOverview: ActionCreator<FetchShardOverview> = () => ({
  type: '@@cluster/fetchShardOverview'
});

export const rebalanceShards: ActionCreator<RebalanceShards> = () => ({
  type: '@@cluster/rebalanceShards'
});

export const moveShard: ActionCreator<MoveShard> = (info: MoveShardInfo) => ({
  type: '@@cluster/moveShard',
  payload: info
});