import React from "react";
import { SelectControl } from "../../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const CaseInput = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <SelectControl
      isDisabled={isDisabled}
      name={`${basePropertiesPath}.case`}
      label="Case"
      selectProps={{
        options: [
          { value: "lower", label: "Lower" },
          { value: "upper", label: "Upper" },
          { value: "none", label: "None" }
        ],
        defaultValue: {
          value: "none",
          label: "None"
        }
      }}
    />
  );
};
