import { useDisclosure } from "@chakra-ui/react";
import { CursorExtras } from "arangojs/cursor";
import React from "react";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";
import { useQueryExecutors } from "./useQueryExecutors";
import { useQueryUpdaters } from "./useQueryUpdaters";
import { useQueryValueModifiers } from "./useQueryValueModifiers";

export type QueryExecutionOptions = {
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
  onSave: (queryName: string) => Promise<void>;
};
// const defaultValue = 'FOR v,e,p IN 1..3 ANY "place/0" GRAPH "ldbc" LIMIT 100 return p'
const QueryContext = React.createContext<QueryContextType>(
  {} as QueryContextType
);

export const useQueryContext = () => React.useContext(QueryContext);

export type QueryResultType = {
  type: "query" | "profile" | "explain";
  result: any;
  extra?: CursorExtras;
};
type CachedQuery = {
  query: string;
  parameter: { [key: string]: string };
};
export const QueryContextProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const initialQueryString = window.sessionStorage.getItem("cachedQuery");
  const initialQuery: CachedQuery = initialQueryString
    ? JSON.parse(initialQueryString)
    : {};
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries();
  const [currentView, setCurrentView] = React.useState<"saved" | "editor">(
    "editor"
  );
  const [queryValue, setQueryValue] = React.useState<string>(
    initialQuery?.query || ""
  );
  const [queryResults, setQueryResults] = React.useState<QueryResultType[]>([]);
  const [queryName, setQueryName] = React.useState<string | undefined>();
  const [queryBindParams, setQueryBindParams] = React.useState<{
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
  const { onSave, onSaveAs } = useQueryUpdaters({
    queryValue,
    queryBindParams,
    savedQueries,
    onCloseSaveAsModal
  });

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
        onCloseSaveAsModal,
        onSave
      }}
    >
      {children}
    </QueryContext.Provider>
  );
};
