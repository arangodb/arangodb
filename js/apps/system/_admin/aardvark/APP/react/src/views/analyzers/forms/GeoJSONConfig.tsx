import { Grid } from "@chakra-ui/react";
import React from "react";
import { GeoTypeInput } from "./inputs/GeoTypeInput";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";

export const GeoJSONConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <GeoTypeInput />
      <GeoOptionsInputs />
    </Grid>
  );
};
