import { SingleSelectControl } from "@arangodb/ui";
import React from "react";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const GeoTypeInput = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <SingleSelectControl
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
