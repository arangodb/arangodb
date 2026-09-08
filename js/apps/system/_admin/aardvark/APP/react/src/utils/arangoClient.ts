import { Database } from "arangojs";
import { memoize } from "lodash";

export const getDB = memoize(
  (db: string) =>
    new Database({
      url: window.location.origin,
      databaseName: db,
      auth: {
        token: window.arangoHelper.getCurrentJwt()
      }
    })
);

export const getCurrentDB = () => getDB(window.frontendConfig.db);
export const getRouteForDB = memoize(
  (db: string, route: string) => getDB(db).route(route),
  (db: string, route: string) => `${db}/${route}`
);

export const getRouteForCurrentDB = (route: string) => {
  return getRouteForDB(window.frontendConfig.db, route);
};

export const getAardvarkRouteForCurrentDb = (url: string) =>
  getRouteForDB(window.frontendConfig.db, `_admin/aardvark/${url}`);
export const getApiRouteForCurrentDB = () =>
  getRouteForDB(window.frontendConfig.db, "_api");
export const getAdminRouteForCurrentDB = () =>
  getRouteForDB(window.frontendConfig.db, "_admin");

// arangod's generic forbidden error number.
const ERROR_FORBIDDEN = 11;

// Classic mode always lets a user read their own access level, so a forbidden
// answer to that probe only happens in RBAC mode, where grants live in roles.
// Callers treat it as "unknown, show everything".
export const isOwnAccessLevelForbidden = (error: unknown) => {
  const e = error as { code?: number; errorNum?: number } | undefined;
  return e?.code === 403 && e?.errorNum === ERROR_FORBIDDEN;
};
