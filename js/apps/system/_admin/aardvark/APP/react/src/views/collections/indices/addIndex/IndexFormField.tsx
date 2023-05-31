import { FormLabel, Spacer } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { MultiSelectControl } from "../../../../components/form/MultiSelectControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { OptionType } from "../../../../components/select/SelectBase";
import { IndexInfoTooltip } from "./IndexInfoTooltip";

export type IndexFormFieldProps = {
  label: string;
  name: string;
  type: string;
  options?: Array<OptionType>;
  tooltip?: string;
  group?: string;
  isRequired?: boolean;
  isDisabled?: boolean;
};

export const IndexFormField = ({
  field,
  render,
  index
}: {
  field: IndexFormFieldProps;
  index?: number;
  render?: (props: {
    field: IndexFormFieldProps;
    index?: number;
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
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label}
          </FormLabel>
          <SwitchControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <IndexInfoTooltip label={field.tooltip} /> : <Spacer />}
        </>
      );
    case "number":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label}
          </FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            inputProps={{ type: "number", autoFocus }}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <IndexInfoTooltip label={field.tooltip} /> : <Spacer />}
        </>
      );
    case "multiSelect":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label}
          </FormLabel>
          <MultiSelectControl
            isDisabled={field.isDisabled}
            selectProps={{
              autoFocus,
              options: field.options
            }}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip ? <IndexInfoTooltip label={field.tooltip} /> : <Spacer />}
        </>
      );
    case "text":
    default:
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label}
          </FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
            inputProps={{ autoFocus }}
          />
          {field.tooltip ? <IndexInfoTooltip label={field.tooltip} /> : <Spacer />}
        </>
      );
  }
};
