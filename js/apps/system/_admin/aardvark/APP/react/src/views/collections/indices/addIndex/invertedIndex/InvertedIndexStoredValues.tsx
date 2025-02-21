import {
  CreatableMultiSelectControl,
  SingleSelectControl,
  SwitchControl
} from "@arangodb/ui";
import { CloseIcon } from "@chakra-ui/icons";
import { Box, Button, FormLabel, IconButton } from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { FormFieldProps } from "../../../../../components/form/FormField";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

export const InvertedIndexStoredValues = ({
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
      <StoredValuesField field={field} />
    </Box>
  );
};

const compressionOptions = [
  {
    label: "LZ4",
    value: "lz4"
  },
  { label: "None", value: "none" }
];

const StoredValuesField = ({ field }: { field: FormFieldProps }) => {
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
                  gridTemplateColumns={"1fr 1fr 70px 30px"}
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
                    <SingleSelectControl
                      isDisabled={field.isDisabled}
                      name={`storedValues.${index}.compression`}
                      selectProps={{
                        options: compressionOptions
                      }}
                    />
                  </Box>
                  <Box minWidth={"0"}>
                    <FormLabel htmlFor={`storedValues.${index}.cache`}>
                      Cache
                    </FormLabel>
                    <SwitchControl
                      switchProps={{
                        isDisabled: !window.frontendConfig.isEnterprise
                      }}
                      name={`storedValues.${index}.cache`}
                      tooltip={
                        window.frontendConfig.isEnterprise
                          ? undefined
                          : "Field normalization value caching is available in the Enterprise Edition."
                      }
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
              colorScheme="green"
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
