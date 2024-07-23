import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { SwitchControl } from "../../../../../components/form/SwitchControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const NGramInputs = ({
  basePath = "properties"
}: {
  basePath?: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <>
      <InputControl
        isDisabled={isDisabled}
        name={`${basePath}.min`}
        label={"Minimum N-Gram Length"}
        inputProps={{
          type: "number",
          min: 1
        }}
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePath}.max`}
        label={"Maximum N-Gram Length"}
        inputProps={{
          type: "number",
          min: 1
        }}
      />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePath}.preserveOriginal`}
        label={"Preserve Original"}
      />
    </>
  );
};
