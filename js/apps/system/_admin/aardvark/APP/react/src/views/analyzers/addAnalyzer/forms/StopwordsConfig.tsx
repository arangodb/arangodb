import { Grid } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { useAnalyzersContext } from "../../AnalyzersContext";

export const StopwordsConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <CreatableMultiSelectControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.stopwords`}
        label="Stopwords"
      />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.hex`}
        label={"Hex"}
      />
    </Grid>
  );
};
