import { InputControl } from "@arangodb/ui";
import { Grid } from "@chakra-ui/react";
import React from "react";
import { useAnalyzersContext } from "../../AnalyzersContext";

export const DelimiterConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Grid templateColumns={"1fr 1fr"} columnGap="4">
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.delimiter`}
        label="Delimiter (characters to split on)"
      />
    </Grid>
  );
};
