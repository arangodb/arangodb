import { Accordion, Box, FormLabel } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";
import { AddNewViewFormValues } from "./AddNewViewForm.types";
import { AdvancedAccordionItem } from "./AdvancedAccordionItem";
import { PrimarySortAccordionItem } from "./PrimarySortAccordionItem";
import { StoredValuesAccordionItem } from "./StoredValuesAccordionItem";

const typeOptions = [
  {
    label: "arangosearch",
    value: "arangosearch"
  },
  {
    label: "search-alias",
    value: "search-alias"
  }
];
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

export const AddNewViewForm = () => {
  return (
    <Box>
      <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
        <FormLabel htmlFor="name">Name</FormLabel>
        <InputControl name="name" />
        <FormLabel htmlFor="type">Type</FormLabel>
        <SelectControl
          name="type"
          selectProps={{
            options: typeOptions
          }}
        />
        <FormLabel htmlFor="primarySortCompression">Primary Sort Compression</FormLabel>
        <SelectControl
          name="primarySortCompression"
          selectProps={{
            options: compressionOptions
          }}
        />
      </Box>
      <ViewTypeForm />
    </Box>
  );
};

const ViewTypeForm = () => {
  const { values } = useFormikContext<AddNewViewFormValues>();
  if (values.type === "arangosearch") {
    return (
      <Accordion marginTop={"4"}>
        <PrimarySortAccordionItem />
      </Accordion>
    );
  }
  return <>search-alias</>;
};
