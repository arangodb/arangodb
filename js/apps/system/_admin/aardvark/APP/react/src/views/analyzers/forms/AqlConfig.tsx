import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { TextareaControl } from "../../../components/form/TextareaControl";

export const AqlConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <TextareaControl name="properties.queryString" label="Query String" />
      <InputControl
        name="properties.batchSize"
        label={"Batch Size"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        name="properties.memoryLimit"
        label={"Memory Limit"}
        inputProps={{
          type: "number"
        }}
      />
      <SwitchControl
        name="properties.collapsePositions"
        label={"Collapse Positions"}
      />
      <SwitchControl name="properties.keepNull" label={"Keep Null"} />
      <SelectControl
        name="properties.returnType"
        label="Return Type"
        selectProps={{
          options: [
            { label: "String", value: "string" },
            { label: "Number", value: "number" },
            { label: "Boolean", value: "bool" }
          ]
        }}
      />
    </Grid>
  );
};
