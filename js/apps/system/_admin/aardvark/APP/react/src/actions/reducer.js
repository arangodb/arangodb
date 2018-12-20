import {
  SHARDS_DETAILS_FETCH,
  SHARDS_DETAILS_FETCH_SUCCESS,
  SHARDS_DETAILS_UNSELECT
} from "./actions";

const reducer = (state, action) => {
  switch (action.type) {
    case SHARDS_DETAILS_FETCH:
      return {...state, selected: action.payload};
    case SHARDS_DETAILS_FETCH_SUCCESS:
      const shardDistribution = {...state.shardDistribution, ...action.payload};
      return {...state, shardDistribution};
    case SHARDS_DETAILS_UNSELECT:
      return {...state, selected: undefined};
    default:
      return state;
  }
};

export default reducer;
