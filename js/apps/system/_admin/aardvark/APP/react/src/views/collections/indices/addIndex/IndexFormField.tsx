import { Box, FormLabel } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { InfoTooltip } from "./InfoTooltip";

export type IndexFormFieldProps = {
  label: string;
  name: string;
  type: string;
  tooltip?: string;
  isRequired?: boolean;
  isDisabled?: boolean;
};

export const IndexFormField = ({
  field,
  render,
  index
}: {
  field: IndexFormFieldProps;
  index: number;
  render?: (props: {
    field: IndexFormFieldProps;
    index: number;
    autoFocus: boolean;
  }) => JSX.Element;
}) => {
  const autoFocus = index === 0;
  if (render && field.type === "custom") {
    return render({ field, index, autoFocus });
  }
  switch (field.type) {
    case "boolean":
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <SwitchControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
        </>
      );
    case "number":
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            inputProps={{ type: "number", autoFocus }}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
        </>
      );
    case "creatableSelect":
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <CreatableMultiSelectControl
            isDisabled={field.isDisabled}
            selectProps={{
              autoFocus,
              noOptionsMessage: () => "Start typing to add a field"
            }}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
        </>
      );
    case "text":
    default:
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
            inputProps={{ autoFocus }}
          />
          {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
        </>
      );
  }
};
