import { Grid } from "@chakra-ui/react";
import React from "react";
import { LocaleInput } from "./inputs/LocaleInput";

export const CollationConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <LocaleInput
        basePropertiesPath={basePropertiesPath}
        placeholder="language[_COUNTRY][_VARIANT][@keywords]"
      />
    </Grid>
  );
};
