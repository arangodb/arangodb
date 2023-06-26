import React from "react";
import { SelectControl } from "../../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const GeoTypeInput = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <SelectControl
      isDisabled={isDisabled}
      name={`${basePropertiesPath}.type`}
      label="Type"
      selectProps={{
        options: [
          { label: "Shape", value: "shape" },
          { label: "Centroid", value: "centroid" },
          { label: "Point", value: "point" }
        ]
      }}
    />
  );
};
