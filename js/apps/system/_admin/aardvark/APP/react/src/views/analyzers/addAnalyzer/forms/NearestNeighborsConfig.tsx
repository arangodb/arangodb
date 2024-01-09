import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { useAnalyzersContext } from "../../AnalyzersContext";

export const NearestNeighborsConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.model_location`}
        label="Model Location"
      />
      <InputControl
        isDisabled={isDisabled}
        inputProps={{
          type: "number"
        }}
        name={`${basePropertiesPath}.top_k`}
        label="Top K"
      />
    </Grid>
  );
};
