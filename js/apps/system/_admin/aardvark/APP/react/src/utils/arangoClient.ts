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

// The classic user-permission API answers this in RBAC mode, where grants
// live in roles instead. Callers treat it as "unknown, show everything".
export const isRbacRejection = (error: unknown) => {
  const e = error as { code?: number; message?: string } | undefined;
  return e?.code === 403 && e?.message === "Not allowed in RBAC mode.";
};
