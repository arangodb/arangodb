import { Grid } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { GeoOptionsInputs } from "./inputs/GeoOptionsInputs";

export const GeopointConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <Grid gridColumn={"1 / -1"} templateColumns={"1fr 1fr 1fr"} columnGap="4">
        <CreatableMultiSelectControl
          isDisabled={isDisabled}
          label={"Latitude Path"}
          name={`${basePropertiesPath}.latitude`}
        />
        <CreatableMultiSelectControl
          isDisabled={isDisabled}
          label={"Longitude Path"}
          name={`${basePropertiesPath}.longitude`}
        />
      </Grid>
      <GeoOptionsInputs basePropertiesPath={basePropertiesPath} />
    </Grid>
  );
};
