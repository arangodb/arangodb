import { useDisclosure } from "@chakra-ui/react";
import React, { useState } from "react";
import { QueryResultType } from "./ArangoQuery.types";
import { QueryType } from "./editor/useFetchUserSavedQueries";
import { QueryKeyboardShortcutProvider } from "./QueryKeyboardShortcutProvider";
import { QueryNavigationPrompt } from "./QueryNavigationPrompt";
import { useQueryEditorHandlers } from "./useQueryEditorHandlers";
import { useQueryExecutors } from "./useQueryExecutors";
import { useQueryResultHandlers } from "./useQueryResultHandlers";
import { useSavedQueriesHandlers } from "./useSavedQueriesHandlers";

export type QueryExecutionOptions = {
  queryValue: string;
  queryBindParams?: { [key: string]: string };
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
};

const QueryContext = React.createContext<QueryContextType>(
  {} as QueryContextType
);

export const useQueryContext = () => React.useContext(QueryContext);

const getStorageKey = () => {
  const currentUser = window.App.currentUser || "root";
  const currentDbName = window.frontendConfig.db;

  return `${currentDbName}-${currentUser}-savedQueries`;
};
export const QueryContextProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const storageKey = getStorageKey();

  const {
    queryValue,
    currentQueryName,
    queryBindParams,
    setCurrentQueryName,
    onQueryChange,
    onQueryValueChange,
    onBindParamsChange
  } = useQueryEditorHandlers();

  const {
    onSave,
    onDelete,
    onSaveAs,
    savedQueries,
    isFetchingQueries,
    isSaveAsModalOpen,
    onOpenSaveAsModal,
    onCloseSaveAsModal
  } = useSavedQueriesHandlers({
    queryValue,
    queryBindParams,
    storageKey
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
        onRemoveAllResults
      }}
    >
      <QueryKeyboardShortcutProvider>
        <QueryNavigationPrompt queryResults={queryResults} />
        {children}
      </QueryKeyboardShortcutProvider>
    </QueryContext.Provider>
  );
};
