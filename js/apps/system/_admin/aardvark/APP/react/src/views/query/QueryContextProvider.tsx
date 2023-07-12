import { useDisclosure, usePrevious } from "@chakra-ui/react";
import { CursorExtras } from "arangojs/cursor";
import hotkeys from "hotkeys-js";
import { isEqual } from "lodash";
import React, { useState } from "react";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";
import { QueryNavigationPrompt } from "./QueryNavigationPrompt";
import { useQueryExecutors } from "./useQueryExecutors";
import { useQueryUpdaters } from "./useQueryUpdaters";
import { useQueryValueModifiers } from "./useQueryValueModifiers";

export type QueryExecutionOptions = {
  queryValue: string;
  queryBindParams?: { [key: string]: string };
};
type QueryContextType = {
  currentView?: "saved" | "editor";
  setCurrentView: (view: "saved" | "editor") => void;
  onExecute: (options: QueryExecutionOptions) => void;
  onProfile: (options: QueryExecutionOptions) => void;
  onExplain: (options: QueryExecutionOptions) => void;
  queryValue: string;
  onQueryValueChange: (value: string) => void;
  onBindParamsChange: (value: { [key: string]: string }) => void;
  onQueryChange: (data: {
    value: string;
    parameter: { [key: string]: string };
    name?: string;
  }) => void;
  queryResults: QueryResultType[];
  setQueryResults: React.Dispatch<React.SetStateAction<QueryResultType[]>>;
  queryBindParams: { [key: string]: string };
  setQueryBindParams: (value: { [key: string]: string }) => void;
  onRemoveResult: (index: number) => void;
  queryName?: string;
  setQueryName: (value: string) => void;
  onSaveAs: (queryName: string) => Promise<void>;
  savedQueries?: QueryType[];
  isFetchingQueries?: boolean;
  isSaveAsModalOpen: boolean;
  onOpenSaveAsModal: () => void;
  onCloseSaveAsModal: () => void;
  onSave: (queryName: string) => Promise<void>;
  resetEditor: boolean;
  setResetEditor: (value: boolean) => void;
  onDelete: (queryName: string) => Promise<void>;
  isSpotlightOpen: boolean;
  setIsSpotlightOpen: (value: boolean) => void;
  aqlJsonEditorRef: React.MutableRefObject<any>;
  bindVariablesJsonEditorRef: React.MutableRefObject<any>;
  setQueryResultById: (queryResult: QueryResultType) => void;
  appendQueryResultById: ({
    asyncJobId,
    result,
    status
  }: {
    asyncJobId?: string;
    result: any;
    status: "success" | "error" | "loading";
  }) => void;
  queryGraphResult: QueryResultType;
  setQueryGraphResult: React.Dispatch<React.SetStateAction<QueryResultType>>;
  queryLimit: number | "all";
  setQueryLimit: (value: number | "all") => void;
};
// const defaultValue = 'FOR v,e,p IN 1..3 ANY "place/0" GRAPH "ldbc" LIMIT 100 return p'
const QueryContext = React.createContext<QueryContextType>(
  {} as QueryContextType
);

export const useQueryContext = () => React.useContext(QueryContext);

export type QueryResultType<ResultType extends any = any> = {
  type: "query" | "profile" | "explain";
  queryValue: string;
  queryBindParams?: { [key: string]: string };
  result?: ResultType;
  extra?: CursorExtras;
  status: "success" | "error" | "loading";
  asyncJobId?: string;
  profile?: any;
  stats?: any;
  warnings?: {
    code: number;
    message: string;
  }[];
  errorMessage?: string;
  queryLimit?: number | "all";
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
  const currentUser = window.App.currentUser || "root";
  const currentDbName = window.frontendConfig.db;
  const storageKey = `${currentDbName}-${currentUser}-savedQueries`;
  const initialQueryString = window.sessionStorage.getItem("cachedQuery");
  const initialQuery = initialQueryString
    ? JSON.parse(initialQueryString)
    : ({} as CachedQuery);
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries({
      storageKey
    });
  const [currentView, setCurrentView] = React.useState<"saved" | "editor">(
    "editor"
  );
  const [queryValue, setQueryValue] = React.useState<string>(
    initialQuery?.query || ""
  );
  const [queryResults, setQueryResults] = React.useState<QueryResultType[]>([]);
  const [queryName, setQueryName] = React.useState<string>("");
  const [queryBindParams, setQueryBindParams] = React.useState<{
    [key: string]: string;
  }>(initialQuery?.parameter || {});

  const { onQueryChange, onQueryValueChange, onBindParamsChange } =
    useQueryValueModifiers({
      setQueryValue,
      setQueryBindParams,
      queryValue,
      setQueryName
    });
  const [resetEditor, setResetEditor] = React.useState<boolean>(false);
  const {
    isOpen: isSaveAsModalOpen,
    onOpen: onOpenSaveAsModal,
    onClose: onCloseSaveAsModal
  } = useDisclosure();
  const { onSave, onSaveAs, onDelete } = useQueryUpdaters({
    queryValue,
    queryBindParams,
    savedQueries,
    storageKey
  });
  const [isSpotlightOpen, setIsSpotlightOpen] = React.useState<boolean>(false);
  const aqlJsonEditorRef = React.useRef(null);
  const bindVariablesJsonEditorRef = React.useRef(null);
  const setQueryResultById = (queryResult: QueryResultType) => {
    setQueryResults(prev => {
      const newResults = prev.map(prevQueryResult => {
        if (prevQueryResult.asyncJobId === queryResult.asyncJobId) {
          return queryResult;
        }
        return prevQueryResult;
      });
      return newResults;
    });
  };
  const appendQueryResultById = ({
    asyncJobId,
    result,
    status
  }: {
    asyncJobId?: string;
    result: any;
    status: "success" | "error" | "loading";
  }) => {
    setQueryResults(prev => {
      const newResults = prev.map(prevQueryResult => {
        if (asyncJobId === prevQueryResult.asyncJobId) {
          return {
            ...prevQueryResult,
            result: [...prevQueryResult.result, ...result],
            status
          };
        }
        return prevQueryResult;
      });
      return newResults;
    });
  };
  const [queryLimit, setQueryLimit] = useState<number | "all">(1000);
  const { onExecute, onProfile, onExplain, onRemoveResult } = useQueryExecutors(
    { setQueryResults, setQueryResultById }
  );
  const [queryGraphResult, setQueryGraphResult] =
    React.useState<QueryResultType>({} as QueryResultType);
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
        onSave,
        resetEditor,
        setResetEditor,
        onDelete,
        setIsSpotlightOpen,
        isSpotlightOpen,
        aqlJsonEditorRef,
        bindVariablesJsonEditorRef,
        setQueryResultById,
        appendQueryResultById,
        queryGraphResult,
        setQueryGraphResult,
        queryLimit,
        setQueryLimit
      }}
    >
      <KeyboardShortcutProvider>
        <QueryNavigationPrompt queryResults={queryResults} />
        {children}
      </KeyboardShortcutProvider>
    </QueryContext.Provider>
  );
};

const KeyboardShortcutProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const {
    onExecute,
    onExplain,
    setIsSpotlightOpen,
    queryValue,
    queryBindParams
  } = useQueryContext();
  const prevQueryBindParams = usePrevious(queryBindParams);
  const areParamsEqual = prevQueryBindParams
    ? isEqual(queryBindParams, prevQueryBindParams)
    : true;
  React.useEffect(() => {
    hotkeys.filter = function (event) {
      var target = event.target;
      var tagName = (target as any)?.tagName;
      return tagName === "INPUT" || tagName === "TEXTAREA";
    };

    hotkeys("ctrl+enter,command+enter", () => {
      onExecute({ queryValue, queryBindParams });
    });

    hotkeys("ctrl+space", () => {
      setIsSpotlightOpen(true);
    });

    hotkeys("ctrl+shift+enter,command+shift+enter", () => {
      onExplain({
        queryValue,
        queryBindParams
      });
    });
    return () => {
      hotkeys.unbind();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [onExecute, queryValue, areParamsEqual]);
  return <>{children}</>;
};
