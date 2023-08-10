import { Box, Flex, Grid, Spacer } from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import { useField } from "formik";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SelectControl } from "../../../../components/form/SelectControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { ANALYZER_TYPE_OPTIONS } from "../../AnalyzersHelpers";
import { AnalyzerTypeForm } from "../AnalyzerTypeForm";

export const MinHashConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Box>
      <AnalyzerField name={`${basePropertiesPath}.analyzer`} />
      <Grid templateColumns={"1fr 1fr"} columnGap="4">
        <InputControl
          isDisabled={isDisabled}
          name={`${basePropertiesPath}.numHashes`}
          label="numHashes"
        />
        <Spacer />
      </Grid>
    </Box>
  );
};

const SUPPORTED_ANALYZER_TYPE_OPTIONS = ANALYZER_TYPE_OPTIONS.filter(
  option => !["minhash", "pipeline"].includes(option.value)
);
const AnalyzerField = ({ name }: { name: string }) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  const [field] =
    useField<Omit<AnalyzerDescription, "name" | "features">>(name);
  return (
    <Flex direction="column" gap="2" padding="2" backgroundColor="gray.100">
      <Grid templateColumns={"1fr 1fr"} columnGap="4">
        <SelectControl
          isDisabled={isDisabled}
          name={`${name}.type`}
          label="Analyzer type"
          selectProps={{
            options: SUPPORTED_ANALYZER_TYPE_OPTIONS
          }}
        />
        <Spacer />
      </Grid>
      <Box paddingX="4">
        <AnalyzerTypeForm
          basePropertiesPath={`${name}.properties`}
          analyzerType={field.value?.type}
        />
      </Box>
    </Flex>
  );
};
