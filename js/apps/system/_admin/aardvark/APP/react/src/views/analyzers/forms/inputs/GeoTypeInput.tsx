import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";

export const GeoTypeInput = () => {
  return (
    <SelectControl
      name="properties.type"
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
