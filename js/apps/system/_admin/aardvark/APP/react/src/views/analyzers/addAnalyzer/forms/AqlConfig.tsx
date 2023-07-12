import { Grid } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SelectControl } from "../../../../components/form/SelectControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { TextareaControl } from "../../../../components/form/TextareaControl";
import { useAnalyzersContext } from "../../AnalyzersContext";

export const AqlConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <TextareaControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.queryString`}
        label="Query String"
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.batchSize`}
        label={"Batch Size"}
        inputProps={{
          type: "number"
        }}
      />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.memoryLimit`}
        label={"Memory Limit"}
        inputProps={{
          type: "number"
        }}
      />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.collapsePositions`}
        label={"Collapse Positions"}
      />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.keepNull`}
        label={"Keep Null"}
      />
      <SelectControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.returnType`}
        label="Return Type"
        selectProps={{
          options: [
            { label: "String", value: "string" },
            { label: "Number", value: "number" },
            { label: "Boolean", value: "bool" }
          ]
        }}
      />
    </Grid>
  );
};
