import { Grid } from "@chakra-ui/react";
import React from "react";
import { LocaleInput } from "./inputs/LocaleInput";

export const StemConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr"} columnGap="4">
      <LocaleInput />
    </Grid>
  );
};
