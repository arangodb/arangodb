import { FormLabel, Spacer } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "./InputControl";
import { MultiSelectControl } from "./MultiSelectControl";
import { SwitchControl } from "./SwitchControl";
import { OptionType } from "../select/SelectBase";
import { IndexInfoTooltip } from "../../views/collections/indices/addIndex/IndexInfoTooltip";

export type FormFieldProps = {
  label: string;
  name: string;
  type: string;
  options?: Array<OptionType>;
  tooltip?: string;
  group?: string;
  isRequired?: boolean;
  isDisabled?: boolean;
};

export const FormField = ({
  field,
  render,
  index
}: {
  field: FormFieldProps;
  index?: number;
  render?: (props: {
    field: FormFieldProps;
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
          {field.tooltip ? (
            <IndexInfoTooltip label={field.tooltip} />
          ) : (
            <Spacer />
          )}
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
          {field.tooltip ? (
            <IndexInfoTooltip label={field.tooltip} />
          ) : (
            <Spacer />
          )}
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
          {field.tooltip ? (
            <IndexInfoTooltip label={field.tooltip} />
          ) : (
            <Spacer />
          )}
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
          {field.tooltip ? (
            <IndexInfoTooltip label={field.tooltip} />
          ) : (
            <Spacer />
          )}
        </>
      );
  }
};
