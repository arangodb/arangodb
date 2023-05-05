import { Grid } from "@chakra-ui/react";
import React from "react";
import { GeoTypeInput } from "./inputs/GeoTypeInput";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";

export const GeoJSONConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1/-1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
        <GeoTypeInput basePropertiesPath={basePropertiesPath} />
      </Grid>

      <GeoOptionsInputs basePropertiesPath={basePropertiesPath} />
    </Grid>
  );
};
