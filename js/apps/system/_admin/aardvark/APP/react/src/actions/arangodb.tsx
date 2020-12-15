import { Database } from "arangojs";
import store from "../configureStore";
import { MoveShardInfo } from '../store/cluster/types';
import { fetchShardDetailsSuccess, checkHealthSuccess } from "../store/cluster/actions";

const dispatchError = (error: {message: string}) => {
  console.log(error);
  // store.dispatch({type: REPORT_ERROR, payload: error}
}

class ArangoDB {
  private db: Database;
  private clusterApi: ReturnType<typeof Database.prototype.route>;

  constructor () {
    let url;

    const env = process.env.NODE_ENV
    if (env === 'development') {
      url = process.env.REACT_APP_ARANGODB_HOST;
    } else {
      url = window.location.origin;
    }

    this.db = new Database({
      url: url
    });
    this.clusterApi = this.db.route('_admin/cluster');
  }

  public async fetchShardSpecifics (collection: string) : Promise<void> {
    const { body, statusCode } = await this.clusterApi.put('collectionShardDistribution', {collection});
    if (statusCode === 200) {
      store.dispatch(fetchShardDetailsSuccess(body.results))
    } else {
      dispatchError({
        message: `Collection ${collection} shard distribution resulted in HTTP Code: ${statusCode}`
      });
    }
  }

  public async fetchShardsOverview () : Promise<void> {
    const { body, statusCode } = await this.clusterApi.get('shardDistribution');
    if (statusCode === 200) {
      store.dispatch(fetchShardDetailsSuccess(body.results))
    } else {
      dispatchError({
        message: `Shard distribution overview resulted in HTTP Code: ${statusCode}`
      });
    }
  }

  public async triggerRebalanceShards () : Promise<void> {
    const { statusCode } = await this.clusterApi.post('rebalanceShards', {});
    if (statusCode !== 201) {
      dispatchError({
        message: `Failed to trigger shard rebalancing. HTTP Code: ${statusCode}`
      });
    }
  }

  public async triggerMoveShard ({collection, shard, fromServer, toServer} : MoveShardInfo) : Promise<void> {
    const { body, statusCode } = await this.clusterApi.post('moveShard', {
      database: this.db.name,
      collection,
      shard,
      fromServer,
      toServer
    });
    if (statusCode !== 202) {
      dispatchError({
        message: `Failed to trigger shard move. HTTP Code: ${statusCode}, response: ${body}`
      });
    }
  }

  public async clusterCheckHealth () : Promise<void> {
    const { body, statusCode } = await this.clusterApi.get('health');
    if (statusCode === 200) {
      store.dispatch(checkHealthSuccess(body.Health));
    }
  }
}

const Instance = new ArangoDB();
const WrappedInstance = new Proxy<ArangoDB>(Instance, {
  // We intercept in all method getters
  get(target: ArangoDB, name: keyof ArangoDB) : any {
    const wrapped : any = target[name];
    if (typeof wrapped === 'function') {
      return async function () {
        try {
          return await wrapped.apply(target, arguments);
        } catch (e) {
          dispatchError(e);
        }
      };
    }
    return wrapped;
  }
});

export default WrappedInstance;
