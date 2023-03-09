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
import { ViewPropertiesType } from "./useFetchViewProperties";
import {
  getUpdatedIndexes,
  useUpdateAliasViewProperties
} from "./useUpdateAliasViewProperties";

type SearchAliasContextType = {
  view: ViewPropertiesType;
  copiedView?: ViewPropertiesType;
  errors: ValidationError[];
  changed: boolean;
  setErrors: (errors: ValidationError[]) => void;
  setView: (view: ViewPropertiesType) => void;
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
  const [view, setView] = useState(initialView);
  const [copiedView, setCopiedView] = useState<
    ViewPropertiesType | undefined
  >();
  const [errors, setErrors] = useState<ValidationError[]>([]);

  const { onSave } = useUpdateAliasViewProperties();
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
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${view.name}`
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
    } catch (e) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };
  const [changed, setChanged] = useState(false);

  useEffect(() => {
    const updatedIndexes = getUpdatedIndexes({ initialView, view });

    if (
      updatedIndexes.changesAdded.length > 0 ||
      updatedIndexes.changesDeleted.length > 0 ||
      initialView.name !== view.name
    ) {
      setChanged(true);
    } else {
      setChanged(false);
    }
  }, [initialView, view]);
  return (
    <SearchAliasContext.Provider
      value={{
        view,
        setView,
        onChange,
        onSave: handleSave,
        errors,
        setErrors,
        changed,
        onCopy: handleCopy,
        copiedView,
        setCopiedView,
        onDelete: handleDelete
      }}
    >
      {children}
    </SearchAliasContext.Provider>
  );
};

export const useSearchAliasContext = () => {
  return useContext(SearchAliasContext);
};
