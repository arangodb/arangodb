import { Grid } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../components/form/CreatableMultiSelectControl";
import { SwitchControl } from "../../../components/form/SwitchControl";

export const StopwordsConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <CreatableMultiSelectControl
        name="properties.stopwords"
        label="Stopwords"
      />
      <SwitchControl name="properties.hex" label={"Hex"} />
    </Grid>
  );
};
