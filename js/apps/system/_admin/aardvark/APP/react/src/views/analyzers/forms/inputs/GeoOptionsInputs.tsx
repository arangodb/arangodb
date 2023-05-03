import React from "react";
import { InputControl } from "../../../../components/form/InputControl";

export const GeoOptionsInputs = () => {
  return (
    <>
      <InputControl
        name={`options.maxCells`}
        label={"Max S2 Cells"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        name={`options.minLevel`}
        label={"Least Precise S2 Level"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        name={`options.maxLevel`}
        label={"Most Precise S2 Level"}
        inputProps={{
          type: "number"
        }}
      />
    </>
  );
};
