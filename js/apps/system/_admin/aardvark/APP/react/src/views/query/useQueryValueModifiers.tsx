import React from "react";
import { parseQuery } from "./queryHelper";

export const useQueryValueModifiers = ({
  setQueryValue,
  setQueryBindParams,
  queryValue,
  setQueryName
}: {
  setQueryValue: (value: string) => void;
  setQueryBindParams: React.Dispatch<
    React.SetStateAction<{ [key: string]: string }>
  >;
  setQueryName: (name: string) => void;
  queryValue: string;
}) => {
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
  return { onQueryChange, onQueryValueChange, onBindParamsChange };
};
