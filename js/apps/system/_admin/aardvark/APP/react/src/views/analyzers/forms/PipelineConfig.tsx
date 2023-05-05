import { AddIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Grid,
  IconButton,
  StatDownArrow,
  StatUpArrow,
  Text
} from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { SelectControl } from "../../../components/form/SelectControl";
import { PipelineStates, TYPE_TO_LABEL_MAP } from "../AnalyzersHelpers";
import { AnalyzerTypeForm } from "../AnalyzerTypeForm";

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
  const { values } = useFormikContext<PipelineStates>();
  console.log("item", values);
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
                  <Box>
                    <SelectControl
                      name={`properties.pipeline.${index}.type`}
                      selectProps={{
                        options: ANALYZER_TYPE_OPTIONS
                      }}
                      label="Type"
                    />
                    <AnalyzerTypeForm analyzerType={item.type} />
                  </Box>

                  <Box marginTop="6">
                    <IconButton
                      aria-label="Delete"
                      variant="ghost"
                      onClick={() => remove(index)}
                      icon={<DeleteIcon />}
                    />
                    <IconButton
                      isDisabled={index === 0}
                      aria-label="Move up"
                      variant="ghost"
                      onClick={() => {}}
                      icon={<StatUpArrow />}
                    />
                    <IconButton
                      isDisabled={
                        index === values.properties.pipeline.length - 1
                      }
                      aria-label="Move down"
                      variant="ghost"
                      onClick={() => {}}
                      icon={<StatDownArrow />}
                    />
                  </Box>
                </Grid>
              );
            })}
            <Button
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
