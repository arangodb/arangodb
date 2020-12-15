import { Reducer } from 'redux';
import { ClusterState, ClusterActions, HealthInfo, ShardDistributionResponse } from './types';

export const initialState: ClusterState = {
  shortToServer: new Map<string, string>(),
  serverToShort: new Map<string, string>(),
  selected: "",
  shardDistribution: {}
};

const createMapsShortVsServer = (health: HealthInfo) => {
  const serverToShort = new Map();
  const shortToServer = new Map();
  for (const [key, value] of Object.entries(health)) {
    if (value.hasOwnProperty('ShortName')) {
      serverToShort.set(key, value.ShortName);
      shortToServer.set(value.ShortName, key);
    }
  }
  return { serverToShort, shortToServer };
};

const mergeShardDistribution = (state: ClusterState, newDistribution : ShardDistributionResponse) : ShardDistributionResponse => {
  const { selected, shardDistribution} = state;
  if (Object.keys(newDistribution).length === 1) {
    if (newDistribution.hasOwnProperty(selected)) {
      return { ...shardDistribution, ...newDistribution };
    }
    // This was an old select, we can ignore
    return shardDistribution;
  }
  // This is a full distribution, just return it
  return newDistribution;
}

const reducer : Reducer<ClusterState> = (state : ClusterState = initialState, action : ClusterActions) => {
  switch (action.type) {
    case '@@cluster/checkHealthSuccess':
      const { serverToShort, shortToServer } = createMapsShortVsServer(action.payload);
      return { ...state, serverToShort, shortToServer };
    case '@@cluster/fetchShardDetails':
      return {...state, selected: action.payload};
    case '@@cluster/fetchShardDetailsSuccess':
      const shardDistribution = mergeShardDistribution(state, action.payload);
      return {...state, shardDistribution};
    case '@@cluster/unselectShardDetails':
      return {...state, selected: ""};
    default:
      return state;
  }
};

export default reducer;