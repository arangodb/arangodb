import { Grid } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../components/form/CreatableMultiSelectControl";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";

export const GeopointConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1 / -1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
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
