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
import { InputControl } from "../../../../components/form/InputControl";
import { SelectControl } from "../../../../components/form/SelectControl";
import { AddNewViewFormValues } from "./AddNewViewForm.types";
import { SwitchControl } from "../../../../components/form/SwitchControl";

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

export const PrimarySortAccordionItem = () => {
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Primary Sort
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <PrimarySortFields />
        <Box
          display={"grid"}
          gridTemplateColumns={"200px 1fr"}
          rowGap="5"
          pt={4}
        >
          <FormLabel htmlFor="primarySortCompression">Compression</FormLabel>
          <SelectControl
            name="primarySortCompression"
            selectProps={{
              options: compressionOptions
            }}
          />
          <FormLabel htmlFor="primarySortCache">Cache</FormLabel>
          <SwitchControl
            name="primarySortCache"
            switchProps={{
              isDisabled: !window.frontendConfig.isEnterprise
            }}
            tooltip={
              window.frontendConfig.isEnterprise
              ? undefined
              : "Primary key column caching is available in Enterprise plans."
            }
          />
        </Box>
      </AccordionPanel>
    </AccordionItem>
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

const PrimarySortFields = () => {
  const { values } = useFormikContext<AddNewViewFormValues>();
  return (
    <Box>
      <FieldArray name="primarySort">
        {({ remove, push }) => (
          <Box display={"grid"} rowGap="4">
            {values.primarySort.map((_, index) => {
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
                    <FormLabel htmlFor={`primarySort.${index}.field`}>
                      Field
                    </FormLabel>
                    <InputControl name={`primarySort.${index}.field`} />
                  </Box>
                  <Box>
                    <FormLabel htmlFor={`primarySort.${index}.direction`}>
                      Direction
                    </FormLabel>
                    <SelectControl
                      name={`primarySort.${index}.direction`}
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
              colorScheme="green"
              onClick={() => {
                push({ field: "", direction: "asc" });
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
