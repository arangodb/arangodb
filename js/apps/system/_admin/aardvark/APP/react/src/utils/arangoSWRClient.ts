import { Database } from 'arangojs';
import useSWR from 'swr';
import { memoize } from 'underscore';

declare var frontendConfig: { [key: string]: any };

const getDB = memoize((db: string) => new Database({
  url: frontendConfig.basePath,
  databaseName: db
}));

const getRoute = memoize((db: string, route: string) => getDB(db).route(route),
  (db: string, route: string) => `${db}/${route}`);

type ApiMethod = 'get' | 'put' | 'post' | 'delete';

const useAPIFetch = (path: string, method: ApiMethod = 'get', body?: any) => useSWR(path, () => {
  const route = getRoute(frontendConfig.db, '_api');

  return route[method](path, body);
});

export default useAPIFetch;
