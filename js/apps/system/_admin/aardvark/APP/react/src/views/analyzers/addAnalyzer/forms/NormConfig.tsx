import { Grid } from "@chakra-ui/react";
import React from "react";
import { AccentInput } from "./inputs/AccentInput";
import { CaseInput } from "./inputs/CaseInput";
import { LocaleInput } from "./inputs/LocaleInput";

export const NormConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4">
      <LocaleInput
        basePropertiesPath={basePropertiesPath}
        placeholder="language[_COUNTRY][_VARIANT]"
      />
      <CaseInput basePropertiesPath={basePropertiesPath} />
      <AccentInput basePropertiesPath={basePropertiesPath} />
    </Grid>
  );
};
