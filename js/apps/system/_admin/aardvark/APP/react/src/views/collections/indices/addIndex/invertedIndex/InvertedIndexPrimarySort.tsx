import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton } from "@chakra-ui/react";
import { FieldArray, useField } from "formik";
import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { SelectControl } from "../../../../../components/form/SelectControl";
import { IndexFormFieldProps } from "../IndexFormField";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

export const InvertedIndexPrimarySort = ({
  field
}: {
  field: IndexFormFieldProps;
}) => {
  return (
    <Box gridColumn="1 / span 3" paddingY="4">
      <PrimarySortFields field={{ ...field, name: "primarySort.fields" }} />
    </Box>
  );
};

const directionOptions = [
  {
    label: "Ascending",
    value: "asc"
  },
  {
    label: "Descending",
    value: "desc"
  }
];

const PrimarySortFields = ({ field }: { field: IndexFormFieldProps }) => {
  const [formikField] = useField<InvertedIndexValuesType[]>(field.name);
  return (
    <Box gridColumn="1 / span 3" paddingY="4" background="gray.100">
      <FormLabel>{field.label}</FormLabel>
      <Box
        display={"grid"}
        gridColumnGap="4"
        gridTemplateColumns={"1fr 1fr 40px"}
        rowGap="5"
        alignItems={"end"}
        marginTop="2"
      >
        <FormLabel htmlFor="primarySort.compression">Compression</FormLabel>
        <SelectControl
          name="primarySort.compression"
          selectProps={{
            options: [
              {
                label: "lz4",
                value: "lz4"
              },
              { label: "None", value: "none" }
            ]
          }}
        />
        <Box></Box>
      </Box>
      <FieldArray name={field.name}>
        {({ push, remove }) => {
          return (
            <>
              {formikField.value?.map((_, index) => {
                return (
                  <Box
                    display={"grid"}
                    gridColumnGap="4"
                    gridTemplateColumns={"1fr 1fr 40px"}
                    rowGap="5"
                    alignItems={"end"}
                    key={index}
                  >
                    <Box>
                      <FormLabel htmlFor={`primarySort.fields.${index}.field`}>
                        Field
                      </FormLabel>
                      <InputControl
                        name={`primarySort.fields.${index}.field`}
                      />
                    </Box>
                    <Box>
                      <FormLabel
                        htmlFor={`primarySort.fields.${index}.direction`}
                      >
                        Direction
                      </FormLabel>
                      <SelectControl
                        name={`primarySort.fields.${index}.direction`}
                        selectProps={{
                          options: directionOptions
                        }}
                      />
                    </Box>
                    {index > 0 ? (
                      <IconButton
                        size="sm"
                        variant="ghost"
                        colorScheme="red"
                        aria-label="Remove field"
                        icon={<CloseIcon />}
                        onClick={() => remove(index)}
                      />
                    ) : null}
                  </Box>
                );
              })}
              <Button
                colorScheme="blue"
                onClick={() => {
                  push({ field: "", direction: "asc" });
                }}
                variant={"ghost"}
                justifySelf="start"
              >
                + Add field
              </Button>
            </>
          );
        }}
      </FieldArray>
    </Box>
  );
};
