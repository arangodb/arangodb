import React from "react";
import { parseQuery } from "./queryHelper";
type CachedQuery = {
  query: string;
  parameter: { [key: string]: string };
};

export const useQueryValueModifiers = () => {
  const initialQueryString = window.sessionStorage.getItem("cachedQuery");
  const initialQuery = initialQueryString
    ? JSON.parse(initialQueryString)
    : ({} as CachedQuery);
  const [queryValue, setQueryValue] = React.useState<string>(
    initialQuery?.query || ""
  );

  const [queryName, setQueryName] = React.useState<string>("");
  const [queryBindParams, setQueryBindParams] = React.useState<{
    [key: string]: string;
  }>(initialQuery?.parameter || {});

  /**
   * Called when the query value is changed by the user
   */
  const onQueryValueChange = (value: string) => {
    const { bindParams: parsedBindParams } = parseQuery(value);
    setQueryValue(value);
    setQueryBindParams(prevQueryBindParams => {
      const parsedBindParamsMap = parsedBindParams.reduce((acc, bindParam) => {
        acc[bindParam] = prevQueryBindParams[bindParam] || "";
        return acc;
      }, {} as { [key: string]: string });
      window.sessionStorage.setItem(
        "cachedQuery",
        JSON.stringify({ query: value, parameter: parsedBindParamsMap })
      );
      return parsedBindParamsMap;
    });
  };
  /**
   * Called when the query bind params are changed by the user (JSON  or form)
   */
  const onBindParamsChange = (value: { [key: string]: string }) => {
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: queryValue, parameter: value })
    );
    setQueryBindParams(value);
  };
  /**
   * Called when setting the query value from saved queries
   * or when creating a new query
   */
  const onQueryChange = ({
    value,
    parameter,
    name
  }: {
    value: string;
    parameter: { [key: string]: string };
    name?: string;
  }) => {
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: value, parameter: parameter })
    );
    setQueryValue(value);
    setQueryBindParams(parameter);
    setQueryName(name || "");
  };
  return {
    onQueryChange,
    onQueryValueChange,
    onBindParamsChange,
    queryValue,
    queryName,
    queryBindParams,
    setQueryBindParams,
    setQueryName
  };
};
