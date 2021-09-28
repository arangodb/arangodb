import { ChangeEvent, useEffect, useRef } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "./arangoClient";
import minimatch from "minimatch";
import { compact } from "lodash";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

export const getChangeHandler = (setter: (value: string) => void) => {
  return (event: ChangeEvent<HTMLInputElement | HTMLSelectElement>) => {
    setter(event.currentTarget.value);
  };
};

export const usePrevious = (value: any) => {
  const ref = useRef();

  useEffect(() => {
    ref.current = value;
  });

  return ref.current;
};


export const usePermissions = () => useSWR(
  `/user/${arangoHelper.getCurrentJwtUsername()}/database/${frontendConfig.db}`,
  (path) => getApiRouteForCurrentDB().get(path)
);

export const facetedFilter = (filterExpr: string, list: { [key: string]: any }[], facets: string[]) => {
  let filteredList = list;

  if (filterExpr) {
    try {
      const filters = filterExpr.trim().split(/\s+/);

      for (const filter of filters) {
        const splitIndex = filter.indexOf(':');
        const field = filter.slice(0, splitIndex);
        const pattern = filter.slice(splitIndex + 1);

        filteredList = filteredList.filter(
          item => minimatch(item[field].toLowerCase(), `*${pattern.toLowerCase()}*`));
      }
    } catch (e) {
      filteredList = filteredList.filter(item => {
        const normalizedPattern = `*${filterExpr.toLowerCase()}*`;

        return facets.some(
          field => minimatch(item[field].toLowerCase(), normalizedPattern));
      });
    }
  }

  return filteredList;
};

export const getPath = (basePath: string | undefined, path: string | undefined) => compact([basePath, path]).join('.');
