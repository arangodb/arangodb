import { Grid } from "@chakra-ui/react";
import React from "react";
import { LocaleInput } from "./inputs/LocaleInput";

export const CollationConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <LocaleInput />
    </Grid>
  );
};
