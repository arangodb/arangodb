import { Formik, useFormikContext } from "formik";
import { ValidationError } from "jsoneditor-react";
import _ from "lodash";
import React, {
  createContext,
  ReactNode,
  useCallback,
  useContext,
  useState
} from "react";
import { useIsAdminUser } from "../../../utils/usePermissions";
import { ViewPropertiesType } from "../searchView.types";
import { useSyncSearchViewUpdates } from "./useSyncSearchViewUpdates";
import { useDeleteView } from "./useDeleteView";

type EditViewContextType = {
  errors: ValidationError[];
  initialView: ViewPropertiesType;
  setChanged: (changed: boolean) => void;
  changed: boolean;
  isAdminUser: boolean;
  isCluster: boolean;
  currentField: string[];
  setCurrentField: (field: string[]) => void;
  currentLink?: string;
  setCurrentLink: (link?: string) => void;
  setErrors: (errors: ValidationError[]) => void;
  onCopy: ({ selectedView }: { selectedView?: ViewPropertiesType }) => void;
  onDelete: () => void;
};
const EditViewContext = createContext<EditViewContextType>(
  {} as EditViewContextType
);

export const EditViewProvider = ({
  initialView,
  children,
  onSave
}: {
  initialView: ViewPropertiesType;
  children: ReactNode;
  onSave: (data: {
    view: ViewPropertiesType;
    initialView: ViewPropertiesType;
    setChanged: (changed: boolean) => void;
  }) => Promise<void>;
}) => {
  const [changed, setChanged] = useState(
    !!window.sessionStorage.getItem(`${initialView.name}-changed`)
  );
  useSyncSearchViewUpdates({ viewName: initialView.name });
  let currentInitialView = initialView;
  const cachedViewStateStr = window.sessionStorage.getItem(initialView.name);
  if (cachedViewStateStr) {
    currentInitialView = JSON.parse(cachedViewStateStr);
  }
  return (
    <Formik
      onSubmit={async values => {
        // call API here
        await onSave({ view: values, initialView, setChanged });
      }}
      initialValues={currentInitialView}
    >
      <EditViewProviderInner
        changed={changed}
        setChanged={setChanged}
        initialView={initialView}
      >
        {children}
      </EditViewProviderInner>
    </Formik>
  );
};

const EditViewProviderInner = ({
  initialView,
  children,
  changed,
  setChanged
}: {
  initialView: ViewPropertiesType;
  children: ReactNode;
  changed: boolean;
  setChanged: (changed: boolean) => void;
}) => {

  const { setValues, values } = useFormikContext<ViewPropertiesType>();
  const isAdminUser = useIsAdminUser();
  const [errors, setErrors] = useState<ValidationError[]>([]);

  const handleCopy = ({
    selectedView
  }: {
    selectedView?: ViewPropertiesType;
  }) => {
    const copiedValues = _.omit(selectedView, "id", "globallyUniqueId", "name");
    setValues({ ...values, ...copiedValues });
  };

  const { onDelete } = useDeleteView({ name: initialView.name });
  const [currentLink, setCurrentLink] = useState<string>();
  const [currentField, setCurrentField] = useState<string[]>([]);
  const handleSetChanged = useCallback(
    (changed: boolean) => {
      if (!changed) {
        window.sessionStorage.removeItem(`${initialView.name}-changed`);
        window.sessionStorage.removeItem(`${initialView.name}`);
      } else {
        window.sessionStorage.setItem(`${initialView.name}-changed`, "true");
        window.sessionStorage.setItem(
          `${initialView.name}`,
          JSON.stringify(values)
        );
      }
      setChanged(changed);
    },
    [initialView.name, setChanged, values]
  );
  return (
    <EditViewContext.Provider
      value={{
        initialView,
        currentField,
        setCurrentField,
        currentLink,
        setCurrentLink,
        errors,
        setErrors,
        setChanged: handleSetChanged,
        changed,
        onCopy: handleCopy,
        onDelete,
        isAdminUser,
        isCluster: !!window.frontendConfig.isCluster
      }}
    >
      {children}
    </EditViewContext.Provider>
  );
};

export const useEditViewContext = () => {
  return useContext(EditViewContext);
};
