import { useDisclosure } from "@chakra-ui/react";
import { aql } from "arangojs";
import { CursorExtras } from "arangojs/cursor";
import React, { createContext, ReactNode, useContext, useState } from "react";
import { mutate } from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";

type QueryExecutionOptions = {
  queryValue?: string;
  queryBindParams?: { [key: string]: string };
};
type QueryContextType = {
  currentView?: "saved" | "editor";
  setCurrentView: (view: "saved" | "editor") => void;
  onExecute: (options: QueryExecutionOptions) => void;
  onProfile: (options: QueryExecutionOptions) => void;
  onExplain: (options: QueryExecutionOptions) => void;
  queryValue?: string;
  onQueryValueChange: (value: string) => void;
  onBindParamsChange: (value: { [key: string]: string }) => void;
  onQueryChange: (data: {
    value: string;
    parameter: { [key: string]: string };
    name?: string;
  }) => void;
  queryResults: QueryResultType[];
  setQueryResults: (value: QueryResultType[]) => void;
  queryBindParams: { [key: string]: string };
  setQueryBindParams: (value: { [key: string]: string }) => void;
  onRemoveResult: (index: number) => void;
  queryName?: string;
  setQueryName: (value?: string) => void;
  onSaveAs: (queryName: string) => void;
  savedQueries?: QueryType[];
  isFetchingQueries?: boolean;
  isSaveAsModalOpen: boolean;
  onOpenSaveAsModal: () => void;
  onCloseSaveAsModal: () => void;
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
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries();
  console.log({ savedQueries });
  const [currentView, setCurrentView] = useState<"saved" | "editor">("editor");
  const [queryValue, setQueryValue] = useState<string>(
    initialQuery?.query || ""
  );
  const [queryResults, setQueryResults] = useState<QueryResultType[]>([]);
  const [queryName, setQueryName] = useState<string | undefined>();
  const [queryBindParams, setQueryBindParams] = useState<{
    [key: string]: string;
  }>(initialQuery?.parameter || {});
  const { onExecute, onProfile, onExplain, onRemoveResult } =
    useQueryExecutors(setQueryResults);
  const { onQueryChange, onQueryValueChange, onBindParamsChange } =
    useQueryValueModifiers({
      setQueryValue,
      setQueryBindParams,
      queryValue,
      setQueryName
    });

  const {
    isOpen: isSaveAsModalOpen,
    onOpen: onOpenSaveAsModal,
    onClose: onCloseSaveAsModal
  } = useDisclosure();
  const onSaveAs = async (queryName: string) => {
    const currentDB = getCurrentDB();
    try {
      await currentDB.request({
        method: "PATCH",
        path: `/_api/user/${encodeURIComponent(window.App.currentUser)}`,
        body: {
          extra: {
            queries: [
              ...(savedQueries || []),
              { name: queryName, value: queryValue, parameter: queryBindParams }
            ]
          }
        }
      });
      window.arangoHelper.arangoNotification(`Saved query: ${queryName}`);
      onCloseSaveAsModal();
      mutate("/savedQueries");
    } catch (e: any) {
      const message = e.message || e.response.body.errorMessage;
      window.arangoHelper.arangoError(
        `Could not save query. Error - ${message}`
      );
    }
  };
  return (
    <QueryContext.Provider
      value={{
        currentView,
        setCurrentView,
        onExecute,
        queryValue,
        onQueryChange,
        onQueryValueChange,
        onBindParamsChange,
        queryResults,
        setQueryResults,
        queryBindParams,
        setQueryBindParams,
        onRemoveResult,
        onProfile,
        onExplain,
        queryName,
        setQueryName,
        onSaveAs,
        savedQueries,
        isFetchingQueries,
        isSaveAsModalOpen,
        onOpenSaveAsModal,
        onCloseSaveAsModal
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

const useQueryExecutors = (
  setQueryResults: React.Dispatch<React.SetStateAction<QueryResultType[]>>
) => {
  const onRemoveResult = (index: number) => {
    setQueryResults(queryResults => {
      const newResults = [...queryResults];
      newResults.splice(index, 1);
      return newResults;
    });
  };
  const onExecute = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    try {
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
    } catch (e: any) {
      const message = e.message || e.response.body.errorMessage;
      window.arangoHelper.arangoError(
        `Could not execute query. Error - ${message}`
      );
    }
  };

  const onProfile = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
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
  const onExplain = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    const literal = aql.literal(queryValue);
    const path = `/_admin/aardvark/query/explain`;
    const route = currentDB.route(path);
    const explainResult = await route.post({
      query: literal.toAQL(),
      bindVars: queryBindParams
    });
    setQueryResults(queryResults => [
      {
        type: "explain",
        result: explainResult.body.msg
      },
      ...queryResults
    ]);
  };
  return { onExecute, onProfile, onExplain, onRemoveResult };
};
const useQueryValueModifiers = ({
  setQueryValue,
  setQueryBindParams,
  queryValue,
  setQueryName
}: {
  setQueryValue: React.Dispatch<React.SetStateAction<string>>;
  setQueryBindParams: React.Dispatch<
    React.SetStateAction<{ [key: string]: string }>
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
    setQueryName(name);
  };
  return { onQueryChange, onQueryValueChange, onBindParamsChange };
};
