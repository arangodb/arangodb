import { useDisclosure } from "@chakra-ui/react";
import React, { useState } from "react";
import { QueryResultType } from "./ArangoQuery.types";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";
import { QueryKeyboardShortcutProvider } from "./QueryKeyboardShortcutProvider";
import { QueryNavigationPrompt } from "./QueryNavigationPrompt";
import { useQueryExecutors } from "./useQueryExecutors";
import { useQueryResultHandlers } from "./useQueryResultHandlers";
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

const getStorageKey = ({
  currentUser,
  currentDbName
}: {
  currentUser?: string;
  currentDbName?: string;
}) => {
  return `${currentDbName}-${currentUser}-savedQueries`;
};
export const QueryContextProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const currentUser = window.App.currentUser || "root";
  const currentDbName = window.frontendConfig.db;
  const storageKey = getStorageKey({
    currentUser,
    currentDbName
  });
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries({
      storageKey
    });

  const {
    queryValue,
    queryName,
    queryBindParams,
    setQueryName,
    onQueryChange,
    onQueryValueChange,
    onBindParamsChange
  } = useQueryValueModifiers();
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
  const {
    queryResults,
    setQueryResults,
    setQueryResultById,
    appendQueryResultById
  } = useQueryResultHandlers();
  const [queryLimit, setQueryLimit] = useState<number | "all">(1000);
  const { onExecute, onProfile, onExplain, onRemoveResult } = useQueryExecutors(
    { setQueryResults, setQueryResultById }
  );
  const [queryGraphResult, setQueryGraphResult] =
    React.useState<QueryResultType>({} as QueryResultType);
  const [currentView, setCurrentView] = React.useState<"saved" | "editor">(
    "editor"
  );
  const aqlJsonEditorRef = React.useRef(null);
  const bindVariablesJsonEditorRef = React.useRef(null);
  const [isSpotlightOpen, setIsSpotlightOpen] = React.useState<boolean>(false);

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
      <QueryKeyboardShortcutProvider>
        <QueryNavigationPrompt queryResults={queryResults} />
        {children}
      </QueryKeyboardShortcutProvider>
    </QueryContext.Provider>
  );
};
