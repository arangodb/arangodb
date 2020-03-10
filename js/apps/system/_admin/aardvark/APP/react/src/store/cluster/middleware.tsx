import { Dispatch, Middleware, MiddlewareAPI } from 'redux';
import arango from '../../actions/arangodb';
import { ClusterActions } from './types';


const middleware : Middleware = (_ : MiddlewareAPI ) =>
      (next : Dispatch<ClusterActions>) =>
      (action : ClusterActions) => {
  switch (action.type) {
    case '@@cluster/fetchShardDetails':
      arango.fetchShardSpecifics(action.payload);
      break;
    case '@@cluster/fetchShardOverview':
      arango.fetchShardsOverview();
      break;
    case '@@cluster/rebalanceShards':
      arango.triggerRebalanceShards();
      break;
    case '@@cluster/moveShard':
      arango.triggerMoveShard(action.payload);
      break;
    case '@@cluster/checkHealth':
      // TODO: enable me when switched to react
      // arango.clusterCheckHealth();
      break;
    default:
  }
  return next(action);
};

export default middleware;
