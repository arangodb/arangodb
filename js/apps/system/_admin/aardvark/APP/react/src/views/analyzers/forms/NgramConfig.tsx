import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";
import { NGramInputs } from "./inputs/NGramInputs";

export const NgramConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4">
      <NGramInputs />
      <InputControl name="properties.startMarker" label="Start Marker" />
      <InputControl name="properties.endMarker" label="End Marker" />
      <SelectControl
        selectProps={{
          options: [
            { label: "Binary", value: "binary" },
            { label: "UTF8", value: "utf8" }
          ]
        }}
        name="properties.streamType"
        label="Stream Type"
      />
    </Grid>
  );
};
