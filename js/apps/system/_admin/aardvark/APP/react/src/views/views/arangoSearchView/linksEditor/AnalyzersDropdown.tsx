import React from "react";
import { MultiSelectControl } from "../../../../components/form/MultiSelectControl";
import { useEditViewContext } from "../../editView/EditViewContext";

export const AnalyzersDropdown = () => {
  const { currentLink } = useEditViewContext();
  return (
    <MultiSelectControl
      name={`links.${currentLink}.analyzers`}
      label="Analyzers"
    />
  );
};
