import React from "react";

const parseQueryParams = (queryValue: string) => {
  // match all values that start with @ (like @variable & @@variable)
  let queryBindParams = queryValue?.match(/@{1,2}\w+/g) as string[];
  // remove the first @ from queryBindParams
  queryBindParams = queryBindParams?.map(param => param.slice(1));
  // convert to object
  return queryBindParams?.reduce((acc, param) => {
    acc[param] = "";
    return acc;
  }, {} as { [key: string]: string; });
};
export const useQueryValueModifiers = ({
  setQueryValue, setQueryBindParams, queryValue, setQueryName
}: {
  setQueryValue: React.Dispatch<React.SetStateAction<string>>;
  setQueryBindParams: React.Dispatch<
    React.SetStateAction<{ [key: string]: string; }>
  >;
  queryValue: string;
  setQueryName: React.Dispatch<React.SetStateAction<string | undefined>>;
}) => {
  const onQueryValueChange = (value: string) => {
    const queryBindParams = parseQueryParams(value);
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: value, parameter: queryBindParams })
    );
    setQueryValue(value);
    setQueryBindParams(queryBindParams || {});
  };
  const onBindParamsChange = (value: { [key: string]: string; }) => {
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: queryValue, parameter: value })
    );
    setQueryBindParams(value);
  };
  const onQueryChange = ({
    value, parameter, name
  }: {
    value: string;
    parameter: { [key: string]: string; };
    name?: string;
  }) => {
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: value, parameter: parameter })
    );
    setQueryValue(value);
    setQueryBindParams(parameter);
    setQueryName(name);
  };
  return { onQueryChange, onQueryValueChange, onBindParamsChange };
};
