import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";

export const NearestNeighborsConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <InputControl name="properties.model_location" label="Model Location" />
      <InputControl
        inputProps={{
          type: "number"
        }}
        name="properties.top_k"
        label="Top K"
      />
    </Grid>
  );
};
