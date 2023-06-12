import { Divider, Stack, Text } from "@chakra-ui/react";
import React from "react";
import { AnalyzerTypes } from "../Analyzer.types";
import { AqlConfig } from "./forms/AqlConfig";
import { ClassificationConfig } from "./forms/ClassificationConfig";
import { CollationConfig } from "./forms/CollationConfig";
import { DelimiterConfig } from "./forms/DelimiterConfig";
import { GeoJSONConfig } from "./forms/GeoJSONConfig";
import { GeopointConfig } from "./forms/GeopointConfig";
import { GeoS2Config } from "./forms/GeoS2Config";
import { NearestNeighborsConfig } from "./forms/NearestNeighborsConfig";
import { NgramConfig } from "./forms/NgramConfig";
import { NormConfig } from "./forms/NormConfig";
import { PipelineConfig } from "./forms/PipelineConfig";
import { SegmentationConfig } from "./forms/SegmentationConfig";
import { StemConfig } from "./forms/StemConfig";
import { StopwordsConfig } from "./forms/StopwordsConfig";
import { TextConfig } from "./forms/TextConfig";

const ANALYZER_TYPE_TO_CONFIG_MAP = {
  identity: null,
  delimiter: DelimiterConfig,
  stem: StemConfig,
  norm: NormConfig,
  ngram: NgramConfig,
  text: TextConfig,
  aql: AqlConfig,
  stopwords: StopwordsConfig,
  collation: CollationConfig,
  segmentation: SegmentationConfig,
  nearest_neighbors: NearestNeighborsConfig,
  classification: ClassificationConfig,
  pipeline: PipelineConfig,
  geojson: GeoJSONConfig,
  geopoint: GeopointConfig,
  geo_s2: GeoS2Config
};

export const AnalyzerTypeForm = ({
  analyzerType,
  basePropertiesPath = "properties"
}: {
  analyzerType: AnalyzerTypes;
  basePropertiesPath?: string;
}) => {
  const AnalyzerConfigComponent = ANALYZER_TYPE_TO_CONFIG_MAP[analyzerType];
  if (!AnalyzerConfigComponent) {
    return null;
  }
  return (
    <Stack gridColumn="1 / -1">
      <Text fontSize="lg">Configuration</Text>
      <Divider />
      <AnalyzerConfigComponent basePropertiesPath={basePropertiesPath} />
    </Stack>
  );
};
