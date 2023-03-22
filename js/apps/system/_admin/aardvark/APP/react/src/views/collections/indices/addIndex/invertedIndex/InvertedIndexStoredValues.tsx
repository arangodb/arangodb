import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton } from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../../components/form/CreatableMultiSelectControl";
import { SelectControl } from "../../../../../components/form/SelectControl";
import { IndexFormFieldProps } from "../IndexFormField";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

export const InvertedIndexStoredValues = ({
  field
}: {
  field: IndexFormFieldProps;
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
      <StoredValuesField field={field} />
    </Box>
  );
};

const compressionOptions = [
  {
    label: "lz4",
    value: "lz4"
  },
  { label: "None", value: "none" }
];

const StoredValuesField = ({ field }: { field: IndexFormFieldProps }) => {
  const { values } = useFormikContext<InvertedIndexValuesType>();
  return (
    <>
      <FieldArray name="storedValues">
        {({ remove, push }) => (
          <Box display={"grid"} rowGap="4">
            {values.storedValues?.map((_, index) => {
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
                  <Box minWidth={"0"}>
                    <FormLabel htmlFor={`storedValues.${index}.fields`}>
                      Fields
                    </FormLabel>
                    <CreatableMultiSelectControl
                      isDisabled={field.isDisabled}
                      selectProps={{
                        noOptionsMessage: () => "Start typing to add a field"
                      }}
                      name={`storedValues.${index}.fields`}
                    />
                  </Box>
                  <Box minWidth={"0"}>
                    <FormLabel htmlFor={`storedValues.${index}.compression`}>
                      Compression
                    </FormLabel>
                    <SelectControl
                      isDisabled={field.isDisabled}
                      name={`storedValues.${index}.compression`}
                      selectProps={{
                        options: compressionOptions
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
                    onClick={() => remove(index)}
                  />
                </Box>
              );
            })}
            <Button
              isDisabled={field.isDisabled}
              colorScheme="blue"
              onClick={() => {
                push({ fields: [], compression: "lz4" });
              }}
              variant={"ghost"}
              justifySelf="start"
            >
              + Add stored value
            </Button>
          </Box>
        )}
      </FieldArray>
    </>
  );
};
