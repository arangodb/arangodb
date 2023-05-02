import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";

export const DelimiterConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr"} columnGap="4">
      <InputControl
        name="properties.delimiter"
        label="Delimiter (characters to split on)"
      />
    </Grid>
  );
};
