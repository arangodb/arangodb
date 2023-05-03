import { Grid } from "@chakra-ui/react";
import React from "react";
import { GeoTypeInput } from "./inputs/GeoTypeInput";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";
import { CreatableMultiSelectControl } from "../../../components/form/CreatableMultiSelectControl";

export const GeopointConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1/-1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
        <CreatableMultiSelectControl
          label={"Latitude Path"}
          name={"properties.latitude"}
        />
        <CreatableMultiSelectControl
          label={"Longitude Path"}
          name={"properties.longitude"}
        />
      </Grid>
      <GeoOptionsInputs />
    </Grid>
  );
};
