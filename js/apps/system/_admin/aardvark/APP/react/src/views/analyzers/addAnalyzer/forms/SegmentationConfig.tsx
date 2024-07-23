import { Grid } from "@chakra-ui/react";
import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { CaseInput } from "./inputs/CaseInput";

export const SegmentationConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <SelectControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.break`}
        selectProps={{
          options: [
            { label: "Alpha", value: "alpha" },
            { label: "All", value: "all" },
            { label: "Graphic", value: "graphic" }
          ]
        }}
        label="Break"
      />
      <CaseInput basePropertiesPath={basePropertiesPath} />
    </Grid>
  );
};
