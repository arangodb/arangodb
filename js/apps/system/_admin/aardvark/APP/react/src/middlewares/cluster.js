import {
  SHARDS_DETAILS_FETCH,
  SHARDS_OVERVIEW_FETCH
} from "../actions/actions";

import arango from "../actions/arangodb";

const clusterMiddleware = (store) => (next) => (action) => {
  switch (action.type) {
    case SHARDS_DETAILS_FETCH:
      // arango.fetchShardSpecifics(action.payload);
      break;
    case SHARDS_OVERVIEW_FETCH:
      // arango.fetchShardsOverview(action.payload);
      break;
    default:
  }
  return next(action);
};

export default clusterMiddleware;
