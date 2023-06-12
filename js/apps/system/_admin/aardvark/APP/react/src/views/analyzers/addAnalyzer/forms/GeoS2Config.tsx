import { Grid } from "@chakra-ui/react";
import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";
import { GeoTypeInput } from "./inputs/GeoTypeInput";

export const GeoS2Config = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1/-1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
        <GeoTypeInput basePropertiesPath={basePropertiesPath} />
        <SelectControl
          isDisabled={isDisabled}
          label="format"
          name={`${basePropertiesPath}.format`}
          selectProps={{
            options: [
              {
                label: "latLngDouble",
                value: "latLngDouble"
              },
              {
                label: "latLngInt",
                value: "latLngInt"
              },
              { label: "s2Point", value: "s2Point" }
            ]
          }}
        />
      </Grid>
      <GeoOptionsInputs basePropertiesPath={basePropertiesPath} />
    </Grid>
  );
};
