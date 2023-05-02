import { Divider, Stack, Text } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { DelimiterConfig } from "./forms/DelimiterConfig";
import { NgramConfig } from "./forms/NgramConfig";
import { NormConfig } from "./forms/NormConfig";
import { StemConfig } from "./forms/StemConfig";
import { TextConfig } from "./forms/TextConfig";

const ANALYZER_TYPE_TO_CONFIG_MAP = {
  identity: null,
  delimiter: DelimiterConfig,
  stem: StemConfig,
  norm: NormConfig,
  ngram: NgramConfig,
  text: TextConfig,
  aql: NormConfig,
  stopwords: NormConfig,
  collation: NormConfig,
  segmentation: NormConfig,
  nearest_neighbors: NormConfig,
  classification: NormConfig,
  pipeline: NormConfig,
  geojson: NormConfig,
  geopoint: NormConfig,
  geo_s2: NormConfig
};
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
