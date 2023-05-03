import { Grid } from "@chakra-ui/react";
import React from "react";
import { SelectControl } from "../../../components/form/SelectControl";
import { CaseInput } from "./inputs/CaseInput";

export const SegmentationConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <SelectControl
        name="properties.break"
        selectProps={{
          options: [
            { label: "Alpha", value: "alpha" },
            { label: "All", value: "all" },
            { label: "Graphic", value: "graphic" }
          ]
        }}
        label="Break"
      />
      <CaseInput />
    </Grid>
  );
};
