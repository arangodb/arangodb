import { ValidationError } from "jsoneditor-react";
import React, {
  createContext,
  ReactNode,
  useContext,
  useEffect,
  useState
} from "react";
import { ViewPropertiesType } from "./useFetchViewProperties";
import {
  getUpdatedIndexes,
  useUpdateAliasViewProperties
} from "./useUpdateAliasViewProperties";

type SearchAliasContextType = {
  view: ViewPropertiesType;
  errors: ValidationError[];
  changed: boolean;
  setErrors: (errors: ValidationError[]) => void;
  setView: (view: ViewPropertiesType) => void;
  onChange: (view: ViewPropertiesType) => void;
  onSave: () => void;
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
  const [errors, setErrors] = useState<ValidationError[]>([]);

  const { onSave } = useUpdateAliasViewProperties();
  const onChange = (view: ViewPropertiesType) => {
    setView(view);
  };

  const handleSave = () => {
    onSave({ view, initialView });
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
        changed
      }}
    >
      {children}
    </SearchAliasContext.Provider>
  );
};

export const useSearchAliasContext = () => {
  return useContext(SearchAliasContext);
};
