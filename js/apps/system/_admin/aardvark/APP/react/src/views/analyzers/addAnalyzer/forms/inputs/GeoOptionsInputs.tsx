import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const GeoOptionsInputs = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <>
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.options.maxCells`}
        label={"Max S2 Cells"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.options.minLevel`}
        label={"Least Precise S2 Level"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.options.maxLevel`}
        label={"Most Precise S2 Level"}
        inputProps={{
          type: "number"
        }}
      />
    </>
  );
};
