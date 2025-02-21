import {
  CreatableMultiSelectControl,
  SingleSelectControl,
  SwitchControl
} from "@arangodb/ui";
import { CloseIcon } from "@chakra-ui/icons";
import {
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  Button,
  FormLabel,
  IconButton
} from "@chakra-ui/react";
import { FieldArray, useFormikContext } from "formik";
import React from "react";
import { AddNewViewFormValues } from "./AddNewViewForm.types";

export const StoredValuesAccordionItem = () => {
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Stored Values
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <StoredValuesFields />
      </AccordionPanel>
    </AccordionItem>
  );
};

const compressionOptions = [
  {
    label: "LZ4",
    value: "lz4"
  },
  {
    label: "None",
    value: "none"
  }
];

const StoredValuesFields = () => {
  const { values } = useFormikContext<AddNewViewFormValues>();
  return (
    <Box>
      <FieldArray name="storedValues">
        {({ remove, push }) => (
          <Box display={"grid"} rowGap="4">
            {values.storedValues.map((_, index) => {
              return (
                <Box
                  display={"grid"}
                  gridColumnGap="4"
                  gridTemplateColumns={"1fr 1fr 70px 30px"}
                  rowGap="5"
                  alignItems={"end"}
                  key={index}
                >
                  <Box minWidth={"0"}>
                    <FormLabel htmlFor={`storedValues.${index}.fields`}>
                      Fields
                    </FormLabel>
                    <CreatableMultiSelectControl
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
                      name={`storedValues.${index}.compression`}
                      selectProps={{
                        options: compressionOptions
                      }}
                    />
                  </Box>
                  <Box minWidth={"0"}>
                    <SwitchControl
                      label="Cache"
                      switchProps={{
                        isDisabled: !window.frontendConfig.isEnterprise
                      }}
                      tooltip={
                        window.frontendConfig.isEnterprise
                          ? undefined
                          : "Field normalization value caching is available in the Enterprise Edition."
                      }
                      name={`storedValues.${index}.cache`}
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
              colorScheme="green"
              onClick={() => {
                push({ field: "", compression: "lz4" });
              }}
              variant={"ghost"}
              justifySelf="start"
            >
              + Add field
            </Button>
          </Box>
        )}
      </FieldArray>
    </Box>
  );
};
