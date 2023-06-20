import { aql } from "arangojs";
import { CursorExtras } from "arangojs/cursor";
import React, { createContext, ReactNode, useContext, useState } from "react";
import { getCurrentDB } from "../../utils/arangoClient";

type QueryContextType = {
  currentView?: "saved" | "editor";
  setCurrentView: (view: "saved" | "editor") => void;
  onExecute?: () => void;
  onProfile?: () => void;
  onExplain?: () => void;
  queryValue?: string;
  onQueryChange: (value: string) => void;
  queryResults: QueryResultType[];
  setQueryResults: (value: QueryResultType[]) => void;
  queryBindParams: { [key: string]: string };
  setQueryBindParams: (value: { [key: string]: string }) => void;
  onRemoveResult: (index: number) => void;
};
// const defaultValue = 'FOR v,e,p IN 1..3 ANY "place/0" GRAPH "ldbc" LIMIT 100 return p'
const QueryContext = createContext<QueryContextType>({} as QueryContextType);

export const useQueryContext = () => useContext(QueryContext);

export type QueryResultType = {
  type: "query" | "profile" | "explain";
  result: any;
  extra?: CursorExtras;
};
type CachedQuery = {
  query: string;
  parameter: { [key: string]: string };
};
export const QueryContextProvider = ({ children }: { children: ReactNode }) => {
  const initialQueryString = window.sessionStorage.getItem("cachedQuery");
  const initialQuery: CachedQuery = initialQueryString
    ? JSON.parse(initialQueryString)
    : {};
  const [currentView, setCurrentView] = useState<"saved" | "editor">("editor");
  const [queryValue, setQueryValue] = useState<string>(
    initialQuery?.query || ""
  );
  const [queryResults, setQueryResults] = useState<QueryResultType[]>([]);
  const [queryBindParams, setQueryBindParams] = useState<{
    [key: string]: string;
  }>(initialQuery?.parameter || {});
  const onExecute = async () => {
    const currentDB = getCurrentDB();
    const cursor = await currentDB.query(
      // An AQL literal created from a normal multi-line string
      aql.literal(queryValue),
      queryBindParams
    );
    const result = await cursor.all();
    const extra = cursor.extra;
    setQueryResults(queryResults => [
      {
        type: "query",
        result,
        extra
      },
      ...queryResults
    ]);
  };
  const handleQueryChange = (value: string) => {
    const queryBindParams = parseQueryParams(value);
    window.sessionStorage.setItem(
      "cachedQuery",
      JSON.stringify({ query: value, parameter: queryBindParams })
    );
    setQueryValue(value);
    setQueryBindParams(queryBindParams || {});
  };
  const onRemoveResult = (index: number) => {
    setQueryResults(queryResults => {
      const newResults = [...queryResults];
      newResults.splice(index, 1);
      return newResults;
    });
  };
  const onProfile = async () => {
    const currentDB = getCurrentDB();
    const literal = aql.literal(queryValue);
    const path = `/_admin/aardvark/query/profile`;
    const route = currentDB.route(path);
    const profile = await route.post({
      query: literal.toAQL(),
      bindVars: queryBindParams
    });
    setQueryResults(queryResults => [
      {
        type: "profile",
        result: profile.body.msg
      },
      ...queryResults
    ]);
  };
  const onExplain = async () => {
    const currentDB = getCurrentDB();
    const literal = aql.literal(queryValue);
    const path = `/_admin/aardvark/query/explain`;
    const route = currentDB.route(path);
    const explainResult = await route.post({
      query: literal.toAQL(),
      bindVars: queryBindParams
    });
    console.log(explainResult);
    setQueryResults(queryResults => [
      {
        type: "explain",
        result: explainResult.body.msg
      },
      ...queryResults
    ]);
  };
  return (
    <QueryContext.Provider
      value={{
        currentView,
        setCurrentView,
        onExecute,
        queryValue,
        onQueryChange: handleQueryChange,
        queryResults,
        setQueryResults,
        queryBindParams,
        setQueryBindParams,
        onRemoveResult,
        onProfile,
        onExplain
      }}
    >
      {children}
    </QueryContext.Provider>
  );
};

const parseQueryParams = (queryValue: string) => {
  // match all values that start with @ (like @variable & @@variable)
  let queryBindParams = queryValue?.match(/@{1,2}\w+/g) as string[];
  // remove the first @ from queryBindParams
  queryBindParams = queryBindParams?.map(param => param.slice(1));
  // convert to object
  return queryBindParams?.reduce((acc, param) => {
    acc[param] = "";
    return acc;
  }, {} as { [key: string]: string });
};
