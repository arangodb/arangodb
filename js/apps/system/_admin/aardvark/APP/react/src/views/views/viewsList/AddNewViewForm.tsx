import { Stack } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";

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

export const AddNewViewForm = () => {
  return (
    <Stack>
      <InputControl labelDirection="row" label="Name" name="name" />
      <SelectControl
        labelDirection="row"
        label="Type"
        name="type"
        selectProps={{
          options: typeOptions
        }}
      />
    </Stack>
  );
};
