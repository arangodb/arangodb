import { Divider, Stack, Text } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { ANALYZER_TYPE_TO_CONFIG_MAP } from "./AnalyzersHelpers";

export const AnalyzerTypeForm = () => {
  const [analyzerTypeField] = useField("type");
  const AnalyzerConfigComponent =
    ANALYZER_TYPE_TO_CONFIG_MAP[
      analyzerTypeField.value as keyof typeof ANALYZER_TYPE_TO_CONFIG_MAP
    ];
  if (!AnalyzerConfigComponent) {
    return null;
  }
  return (
    <Stack gridColumn="1 / -1">
      <Text fontSize="lg">Configuration</Text>
      <Divider />
      <AnalyzerConfigComponent />
    </Stack>
  );
};
