import { useDisclosure } from "@chakra-ui/react";
import { QueryInfo } from "arangojs/database";
import React, { useState } from "react";
import { QueryResultType } from "./ArangoQuery.types";
import { QueryType } from "./editor/useFetchUserSavedQueries";
import { QueryKeyboardShortcutProvider } from "./QueryKeyboardShortcutProvider";
import { QueryNavigationPrompt } from "./QueryNavigationPrompt";
import { useFetchRunningQueries } from "./runningQueries/useFetchRunningQueries";
import { useQueryEditorHandlers } from "./useQueryEditorHandlers";
import { useQueryExecutors } from "./useQueryExecutors";
import { useQueryResultHandlers } from "./useQueryResultHandlers";
import { useSavedQueriesHandlers } from "./useSavedQueriesHandlers";

export type QueryExecutionOptions = {
  queryValue: string;
  queryBindParams?: { [key: string]: string };
  queryOptions?: { [key: string]: any };
  disabledRules?: string[];
};
type QueryContextType = {
  queryValue: string;
  queryBindParams: { [key: string]: string };
  onQueryValueChange: (value: string) => void;
  onBindParamsChange: (value: { [key: string]: string }) => void;
  onQueryChange: (data: {
    value: string;
    parameter: { [key: string]: string };
    name?: string;
  }) => void;
  onExecute: (options: QueryExecutionOptions) => void;
  onProfile: (options: QueryExecutionOptions) => void;
  onExplain: (options: QueryExecutionOptions) => void;
  queryResults: QueryResultType[];
  onRemoveResult: (index: number) => void;
  onRemoveAllResults: () => void;
  currentView?: "saved" | "editor";
  setCurrentView: (view: "saved" | "editor") => void;
  currentQueryName?: string;
  setCurrentQueryName: (value: string) => void;
  onSave: (queryName: string) => Promise<void>;
  onDelete: (queryName: string) => Promise<void>;
  onSaveAs: (queryName: string) => Promise<void>;
  onSaveQueryList: (queries: QueryType[]) => Promise<void>;
  isSaveAsModalOpen: boolean;
  onOpenSaveAsModal: () => void;
  onCloseSaveAsModal: () => void;
  savedQueries?: QueryType[];
  isFetchingQueries?: boolean;
  resetEditor: boolean;
  setResetEditor: (value: boolean) => void;
  isSpotlightOpen: boolean;
  onOpenSpotlight: () => void;
  onCloseSpotlight: () => void;
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
  runningQueries?: QueryInfo[];
  refetchRunningQueries: () => Promise<void>;
  queryOptions: { [key: string]: any };
  setQueryOptions: React.Dispatch<React.SetStateAction<{ [key: string]: any }>>;
  disabledRules: string[];
  setDisabledRules: React.Dispatch<React.SetStateAction<string[]>>;
};

const QueryContext = React.createContext<QueryContextType>(
  {} as QueryContextType
);

export const useQueryContext = () => React.useContext(QueryContext);

export const QueryContextProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const {
    queryValue,
    currentQueryName,
    queryBindParams,
    setCurrentQueryName,
    onQueryChange,
    onQueryValueChange,
    onBindParamsChange,
    disabledRules,
    setDisabledRules,
    queryOptions,
    setQueryOptions
  } = useQueryEditorHandlers();

  const {
    onSave,
    onDelete,
    onSaveAs,
    savedQueries,
    isFetchingQueries,
    isSaveAsModalOpen,
    onOpenSaveAsModal,
    onCloseSaveAsModal,
    onSaveQueryList
  } = useSavedQueriesHandlers({
    queryValue,
    queryBindParams
  });
  const {
    queryResults,
    setQueryResults,
    setQueryResultById,
    appendQueryResultById,
    onRemoveResult,
    onRemoveAllResults
  } = useQueryResultHandlers();
  const { onExecute, onProfile, onExplain } = useQueryExecutors({
    setQueryResults,
    setQueryResultById
  });
  const aqlJsonEditorRef = React.useRef(null);
  const bindVariablesJsonEditorRef = React.useRef(null);
  const [queryLimit, setQueryLimit] = useState<number | "all">(1000);
  const [queryGraphResult, setQueryGraphResult] =
    React.useState<QueryResultType>({} as QueryResultType);
  const [currentView, setCurrentView] = React.useState<"saved" | "editor">(
    "editor"
  );
  const [resetEditor, setResetEditor] = React.useState<boolean>(false);
  const {
    isOpen: isSpotlightOpen,
    onOpen: onOpenSpotlight,
    onClose: onCloseSpotlight
  } = useDisclosure();
  const { runningQueries, refetchRunningQueries } = useFetchRunningQueries();

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
        queryBindParams,
        onRemoveResult,
        onProfile,
        onExplain,
        currentQueryName,
        setCurrentQueryName,
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
        isSpotlightOpen,
        onOpenSpotlight,
        onCloseSpotlight,
        aqlJsonEditorRef,
        bindVariablesJsonEditorRef,
        setQueryResultById,
        appendQueryResultById,
        queryGraphResult,
        setQueryGraphResult,
        queryLimit,
        setQueryLimit,
        onRemoveAllResults,
        onSaveQueryList,
        runningQueries,
        refetchRunningQueries,
        queryOptions,
        setQueryOptions,
        disabledRules,
        setDisabledRules
      }}
    >
      <QueryKeyboardShortcutProvider>
        <QueryNavigationPrompt queryResults={queryResults} />
        {children}
      </QueryKeyboardShortcutProvider>
    </QueryContext.Provider>
  );
};
