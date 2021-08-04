import { Database } from 'arangojs';
import useSWR from 'swr';
import { memoize } from 'lodash';

declare var frontendConfig: { [key: string]: any };

export const getDB = memoize((db: string) => new Database({
  url: frontendConfig.basePath,
  databaseName: db
}));

export const getRouteForDB = memoize((db: string, route: string) => getDB(db).route(route),
  (db: string, route: string) => `${db}/${route}`);

export const getApiRouteForCurrentDB = () => getRouteForDB(frontendConfig.db, '_api');

type ApiMethod = 'get' | 'put' | 'post' | 'delete';

export const useAPIFetch = (path: string | null, method: ApiMethod = 'get', body?: any) => useSWR(path, () => {
  const route = getApiRouteForCurrentDB();

  return route[method](path as string, body);
});
