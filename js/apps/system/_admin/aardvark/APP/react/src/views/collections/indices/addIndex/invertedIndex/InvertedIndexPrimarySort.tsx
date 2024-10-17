import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton, Spacer } from "@chakra-ui/react";
import { FieldArray, useField } from "formik";
import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { SelectControl } from "../../../../../components/form/SelectControl";
import { FormFieldProps } from "../../../../../components/form/FormField";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";
import { SwitchControl } from "../../../../../components/form/SwitchControl";

export const InvertedIndexPrimarySort = ({
  field
}: {
  field: FormFieldProps;
}) => {
  return (
    <Box
      gridColumn="1 / span 3"
      border="2px solid"
      borderRadius="md"
      borderColor="gray.200"
      padding="4"
      marginX="-4"
    >
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

const PrimarySortFields = ({ field }: { field: FormFieldProps }) => {
  const [formikField] = useField<InvertedIndexValuesType[]>(field.name);
  return (
    <>
      <Box
        display={"grid"}
        gridColumnGap="4"
        gridTemplateColumns="200px 1fr 40px"
        rowGap="5"
        alignItems={"end"}
        marginTop="2"
      >
        <FormLabel htmlFor="primarySort.compression">Compression</FormLabel>
        <SelectControl
          isDisabled={field.isDisabled}
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
        <Spacer />
        <FormLabel htmlFor="primarySort.cache">Cache</FormLabel>
        <SwitchControl
          switchProps={{
            isDisabled: field.isDisabled || !window.frontendConfig.isEnterprise
          }}
          name="primarySort.cache"
          tooltip={
            window.frontendConfig.isEnterprise
            ? undefined
            : "Primary sort column caching is available in Enterprise plans."
          }
        />
        <Spacer />
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
                    marginTop="4"
                  >
                    <Box>
                      <FormLabel htmlFor={`primarySort.fields.${index}.field`}>
                        Field
                      </FormLabel>
                      <InputControl
                        isDisabled={field.isDisabled}
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
                        isDisabled={field.isDisabled}
                        name={`primarySort.fields.${index}.direction`}
                        selectProps={{
                          options: directionOptions
                        }}
                      />
                    </Box>

                    <IconButton
                      isDisabled={field.isDisabled}
                      size="sm"
                      variant="ghost"
                      colorScheme="red"
                      aria-label="Remove field"
                      icon={<CloseIcon />}
                      onClick={() => {
                        remove(index);
                      }}
                    />
                  </Box>
                );
              })}

              <Button
                isDisabled={field.isDisabled}
                marginTop="4"
                colorScheme="green"
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
    </>
  );
};
