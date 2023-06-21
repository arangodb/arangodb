import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SelectControl } from "../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { NGramInputs } from "./inputs/NGramInputs";

export const NgramConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4">
      <NGramInputs />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.startMarker`}
        label="Start Marker"
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.endMarker`}
        label="End Marker"
      />
      <SelectControl
        isDisabled={isDisabled}
        selectProps={{
          options: [
            { label: "Binary", value: "binary" },
            { label: "UTF8", value: "utf8" }
          ]
        }}
        name={`${basePropertiesPath}.streamType`}
        label="Stream Type"
      />
    </Grid>
  );
};
