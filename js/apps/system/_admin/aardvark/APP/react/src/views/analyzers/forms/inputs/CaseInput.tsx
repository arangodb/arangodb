import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";

export const CaseInput = () => {
  return (
    <SelectControl
      name="properties.case"
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
