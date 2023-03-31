import { Database } from 'arangojs';
import { memoize } from 'lodash';

const env = process.env.NODE_ENV;
let url: string;
if (env === 'development') {
  url = process.env.REACT_APP_ARANGODB_HOST as string;
} else {
  url = window.location.origin;
}

export const getDB = memoize((db: string) => new Database({
  url,
  databaseName: db,
  auth: {
    token: window.arangoHelper.getCurrentJwt()
  }
}));

export const getRouteForDB = memoize((db: string, route: string) => getDB(db).route(route),
  (db: string, route: string) => `${db}/${route}`);

export const getApiRouteForCurrentDB = () => getRouteForDB(window.frontendConfig.db, '_api');
