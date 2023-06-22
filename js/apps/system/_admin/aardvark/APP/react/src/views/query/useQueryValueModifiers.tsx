import React from "react";
import { getCurrentDB } from "../../utils/arangoClient";

const parseQueryParams = async (queryValue: string) => {
  const currentDB = getCurrentDB();
  const parsed = await currentDB.parse(queryValue);
  const bindVarsMap = parsed.bindVars.reduce((acc, bindVar) => {
    acc[bindVar] = "";
    return acc;
  }, {} as { [key: string]: string });
  return bindVarsMap;
};
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
  queryValue: string;
  setQueryName: (name: string) => void;
}) => {
  const onQueryValueChange = async (value: string) => {
    setQueryValue(value);
    const queryBindParams = await parseQueryParams(value);
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: value, parameter: queryBindParams })
    );
    setQueryBindParams(queryBindParams || {});
  };
  const onBindParamsChange = (value: { [key: string]: string }) => {
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: queryValue, parameter: value })
    );
    setQueryBindParams(value);
  };
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
