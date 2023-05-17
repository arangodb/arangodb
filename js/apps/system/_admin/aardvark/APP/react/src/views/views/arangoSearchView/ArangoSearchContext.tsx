import { ValidationError } from "jsoneditor-react";
import React, { createContext, ReactNode, useContext, useState } from "react";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useIsAdminUser } from "../../../utils/usePermissions";
import { ArangoSearchViewPropertiesType } from "../searchView.types";
import { useSyncSearchViewUpdates } from "../useSyncSearchViewUpdates";

type ArangoSearchContextType = {
  view: ArangoSearchViewPropertiesType;
  initialView: ArangoSearchViewPropertiesType;
  copiedView?: ArangoSearchViewPropertiesType;
  errors: ValidationError[];
  changed: boolean;
  isAdminUser: boolean;
  isCluster: boolean;
  currentName?: string;
  setErrors: (errors: ValidationError[]) => void;
  setView: (view: ArangoSearchViewPropertiesType) => void;
  setCurrentName: (name?: string) => void;
  setCopiedView: (view: ArangoSearchViewPropertiesType | undefined) => void;
  onChange: (view: ArangoSearchViewPropertiesType) => void;
  onCopy: ({
    selectedView
  }: {
    selectedView?: ArangoSearchViewPropertiesType;
  }) => void;
  onSave: () => void;
  onDelete: () => void;
};
const ArangoSearchContext = createContext<ArangoSearchContextType>(
  {} as ArangoSearchContextType
);

export const ArangoSearchPropertiesProvider = ({
  initialView,
  children
}: {
  initialView: ArangoSearchViewPropertiesType;
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
    ArangoSearchViewPropertiesType | undefined
  >();
  const [errors, setErrors] = useState<ValidationError[]>([]);
  const [changed] = useState(
    !!window.sessionStorage.getItem(`${initialView.name}-changed`)
  );
  useSyncSearchViewUpdates({ viewName: view.name });

  const onChange = (view: ArangoSearchViewPropertiesType) => {
    setView(view);
  };

  const handleCopy = ({
    selectedView
  }: {
    selectedView?: ArangoSearchViewPropertiesType;
  }) => {
    console.log("copy", selectedView);
  };

  const handleSave = () => {
    console.log("save");
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
  return (
    <ArangoSearchContext.Provider
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
    </ArangoSearchContext.Provider>
  );
};

export const useArangoSearchContext = () => {
  return useContext(ArangoSearchContext);
};
