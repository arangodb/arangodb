import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";

export const NGramInputs = ({
  basePath = "properties"
}: {
  basePath?: string;
}) => {
  return (
    <>
      <InputControl
        name={`${basePath}.min`}
        label={"Minimum N-Gram Length"}
        inputProps={{
          type: "number",
          min: 1
        }}
      />
      <InputControl
        name={`${basePath}.max`}
        label={"Maximum N-Gram Length"}
        inputProps={{
          type: "number",
          min: 1
        }}
      />
      <SwitchControl
        name={`${basePath}.preserveOriginal`}
        label={"Preserve Original"}
      />
    </>
  );
};
