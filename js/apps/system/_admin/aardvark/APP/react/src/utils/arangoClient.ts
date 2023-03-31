import { Database } from 'arangojs';
import { memoize } from 'lodash';

export const getDB = memoize((db: string) => new Database({
  url: window.location.origin,
  databaseName: db,
  auth: {
    token: window.arangoHelper.getCurrentJwt()
  }
}));

export const getRouteForDB = memoize((db: string, route: string) => getDB(db).route(route),
  (db: string, route: string) => `${db}/${route}`);

export const getApiRouteForCurrentDB = () => getRouteForDB(window.frontendConfig.db, '_api');
