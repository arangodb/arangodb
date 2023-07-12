import {
  AddIcon,
  ArrowDownIcon,
  ArrowUpIcon,
  DeleteIcon
} from "@chakra-ui/icons";
import { Box, Button, Grid, IconButton, Stack, Text } from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { SelectControl } from "../../../../components/form/SelectControl";
import { TYPE_TO_LABEL_MAP } from "../../AnalyzersHelpers";
import { PipelineStates } from "../../Analyzer.types";
import { AnalyzerTypeForm } from "../AnalyzerTypeForm";
import { useAnalyzersContext } from "../../AnalyzersContext";

const ANALYZER_TYPE_OPTIONS = TYPE_TO_LABEL_MAP
  ? (Object.keys(TYPE_TO_LABEL_MAP)
      .map(type => {
        const excludedTypes = ["pipeline", "geo_s2", "geojson", "geopoint"];
        if (excludedTypes.includes(type)) return null;
        return {
          value: type,
          label: TYPE_TO_LABEL_MAP[type as keyof typeof TYPE_TO_LABEL_MAP]
        };
      })
      .filter(Boolean) as { label: string; value: string }[])
  : [];

export const PipelineConfig = () => {
  const { values, setValues } = useFormikContext<PipelineStates>();
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  return (
    <FieldArray name="properties.pipeline">
      {({ remove, push }) => {
        if (!values.properties?.pipeline) {
          return null;
        }
        return (
          <Grid
            autoRows={
              values.properties.pipeline.length > 1 ? "auto" : "min-content"
            }
          >
            {values.properties.pipeline.map((item, index) => {
              return (
                <Grid
                  key={index}
                  templateColumns={"30px 1fr 180px"}
                  columnGap="4"
                  alignItems="flex-start"
                  backgroundColor="gray.100"
                  padding="4"
                  borderBottomWidth="1px"
                  borderBottomColor="gray.300"
                >
                  <Text>{index + 1}.</Text>
                  <Stack>
                    <SelectControl
                      isDisabled={isDisabled}
                      name={`properties.pipeline.${index}.type`}
                      selectProps={{
                        options: ANALYZER_TYPE_OPTIONS
                      }}
                      label="Type"
                    />
                    <AnalyzerTypeForm
                      basePropertiesPath={`properties.pipeline.${index}.properties`}
                      analyzerType={item.type}
                    />
                  </Stack>

                  <Box marginTop="6">
                    <IconButton
                      aria-label="Delete"
                      variant="ghost"
                      isDisabled={isDisabled}
                      onClick={() => remove(index)}
                      icon={<DeleteIcon />}
                    />
                    <IconButton
                      isDisabled={index === 0 || isDisabled}
                      aria-label="Move up"
                      variant="ghost"
                      onClick={() => {
                        const { pipeline } = values.properties;
                        const newPipeline = [
                          ...pipeline.slice(0, index - 1),
                          pipeline[index],
                          pipeline[index - 1],
                          ...pipeline.slice(index + 1)
                        ];
                        setValues({
                          ...values,
                          properties: {
                            ...values.properties,
                            pipeline: newPipeline
                          }
                        });
                      }}
                      icon={<ArrowUpIcon />}
                    />
                    <IconButton
                      isDisabled={
                        index === values.properties.pipeline.length - 1 ||
                        isDisabled
                      }
                      aria-label="Move down"
                      variant="ghost"
                      onClick={() => {
                        const { pipeline } = values.properties;
                        const newPipeline = [
                          ...pipeline.slice(0, index),
                          pipeline[index + 1],
                          pipeline[index],
                          ...pipeline.slice(index + 2)
                        ];
                        setValues({
                          ...values,
                          properties: {
                            ...values.properties,
                            pipeline: newPipeline
                          }
                        });
                      }}
                      icon={<ArrowDownIcon />}
                    />
                  </Box>
                </Grid>
              );
            })}
            <Button
              isDisabled={isDisabled}
              onClick={() => push({ type: "identity" })}
              variant="ghost"
              colorScheme="blue"
              leftIcon={<AddIcon />}
            >
              Add analyzer
            </Button>
          </Grid>
        );
      }}
    </FieldArray>
  );
};
