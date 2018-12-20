import { Database } from "arangojs";
import store from "./store";
import { SHARDS_DETAILS_FETCH_SUCCESS } from "./actions";

class ArangoDB {
  constructor () {
    this.db = new Database({
      url: 'http://localhost:8529'
    });
    this.clusterApi = this.db.route('_admin/cluster');
  }

  async fetchShardSpecifics (collection) {
    try {
      const { body, statusCode } = await this.clusterApi.put('collectionShardDistribution', {collection});
      if (statusCode === 200) {
        store.dispatch({
          type: SHARDS_DETAILS_FETCH_SUCCESS,
          payload: body.results
        });
      } else {
        console.log("el fail");
        throw "banana";
      }
    } catch (e) {
        console.log("el fail", e);
    }
  }

  async fetchShardsOverview () {
    try {
      const { body, statusCode } = await this.clusterApi.get('shardDistribution');
      if (statusCode === 200) {
        store.dispatch({
          type: SHARDS_DETAILS_FETCH_SUCCESS,
          payload: body.results
        });
      } else {
        console.log("el fail");
        throw "banana";
      }
    } catch (e) {
        console.log("el fail", e);
    }

  }
}

const Instance = new ArangoDB();
export default Instance;
