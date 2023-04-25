import { ValidationError } from "jsoneditor-react";
import React, {
  createContext,
  ReactNode,
  useContext,
  useEffect,
  useState
} from "react";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useIsAdminUser } from "../../../utils/usePermissions";
import { ViewPropertiesType } from "./useFetchViewProperties";
import { useSyncSearchViewUpdates } from "./useSyncSearchViewUpdates";
import {
  getUpdatedIndexes,
  useUpdateAliasViewProperties
} from "./useUpdateAliasViewProperties";

type SearchAliasContextType = {
  view: ViewPropertiesType;
  initialView: ViewPropertiesType;
  copiedView?: ViewPropertiesType;
  errors: ValidationError[];
  changed: boolean;
  isAdminUser: boolean;
  isCluster: boolean;
  currentName?: string;
  setErrors: (errors: ValidationError[]) => void;
  setView: (view: ViewPropertiesType) => void;
  setCurrentName: (name?: string) => void;
  setCopiedView: (view: ViewPropertiesType | undefined) => void;
  onChange: (view: ViewPropertiesType) => void;
  onCopy: ({ selectedView }: { selectedView?: ViewPropertiesType }) => void;
  onSave: () => void;
  onDelete: () => void;
};
const SearchAliasContext = createContext<SearchAliasContextType>(
  {} as SearchAliasContextType
);

export const SearchAliasProvider = ({
  initialView,
  children
}: {
  initialView: ViewPropertiesType;
  children: ReactNode;
}) => {
  const isAdminUser = useIsAdminUser();
  let newInitalView = initialView;
  const cachedViewStateStr = window.sessionStorage.getItem(initialView.name);
  if (cachedViewStateStr) {
    newInitalView = JSON.parse(cachedViewStateStr);
  }
  const [view, setView] = useState(newInitalView);
  const [currentName, setCurrentName] = useState<string | undefined>(
    newInitalView.name
  );
  const [copiedView, setCopiedView] = useState<
    ViewPropertiesType | undefined
  >();
  const [errors, setErrors] = useState<ValidationError[]>([]);
  const [changed, setChanged] = useState(
    !!window.sessionStorage.getItem(`${initialView.name}-changed`)
  );
  const { onSave } = useUpdateAliasViewProperties({ setChanged });
  useSyncSearchViewUpdates({ viewName: view.name });

  const onChange = (view: ViewPropertiesType) => {
    setView(view);
  };

  const handleCopy = ({
    selectedView
  }: {
    selectedView?: ViewPropertiesType;
  }) => {
    selectedView?.indexes &&
      setCopiedView({ ...view, indexes: selectedView.indexes });
  };

  const handleSave = () => {
    onSave({ view, initialView });
  };

  const handleDelete = async () => {
    try {
      const { encoded: encodedViewName } = encodeHelper(view.name);
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${encodedViewName}`
      );

      if (result.body.error) {
        window.arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
      } else {
        mutate("/view");
        window.App.navigate("#views", { trigger: true });
        window.arangoHelper.arangoNotification(
          "Success",
          `Deleted View: ${view.name}`
        );
      }
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  useEffect(() => {
    const updatedIndexes = getUpdatedIndexes({ initialView, view });

    if (
      updatedIndexes.addedChanges.length > 0 ||
      updatedIndexes.deletedChanges.length > 0 ||
      initialView.name !== view.name
    ) {
      setChanged(true);
      window.sessionStorage.setItem(`${initialView.name}-changed`, "true");
      window.sessionStorage.setItem(
        `${initialView.name}`,
        JSON.stringify(view)
      );
    } else {
      setChanged(false);
      window.sessionStorage.removeItem(`${initialView.name}-changed`);
      window.sessionStorage.removeItem(`${initialView.name}`);
    }
  }, [initialView, view]);

  return (
    <SearchAliasContext.Provider
      value={{
        view,
        initialView,
        setView,
        onChange,
        onSave: handleSave,
        errors,
        setErrors,
        changed,
        onCopy: handleCopy,
        copiedView,
        setCopiedView,
        onDelete: handleDelete,
        isAdminUser,
        isCluster: !!window.frontendConfig.isCluster,
        setCurrentName,
        currentName
      }}
    >
      {children}
    </SearchAliasContext.Provider>
  );
};

export const useSearchAliasContext = () => {
  return useContext(SearchAliasContext);
};
