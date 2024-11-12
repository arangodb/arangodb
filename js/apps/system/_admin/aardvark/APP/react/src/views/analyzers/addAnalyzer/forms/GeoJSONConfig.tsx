import { SwitchControl } from "@arangodb/ui";
import { Grid } from "@chakra-ui/react";
import React from "react";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";
import { GeoTypeInput } from "./inputs/GeoTypeInput";

export const GeoJSONConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1/-1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
        <GeoTypeInput basePropertiesPath={basePropertiesPath} />
      </Grid>
      <GeoOptionsInputs basePropertiesPath={basePropertiesPath} />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.legacy`}
        label="Legacy"
      />
    </Grid>
  );
};
