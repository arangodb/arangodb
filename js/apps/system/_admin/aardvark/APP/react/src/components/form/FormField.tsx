import { FormLabel, Spacer } from "@chakra-ui/react";
import React, { ReactNode } from "react";
import { IndexInfoTooltip } from "../../views/collections/indices/addIndex/IndexInfoTooltip";
import { OptionType } from "../select/SelectBase";
import { CreatableMultiSelectControl } from "./CreatableMultiSelectControl";
import { CreatableSingleSelectControl } from "./CreatableSingleSelectControl";
import { InputControl } from "./InputControl";
import { MultiSelectControl } from "./MultiSelectControl";
import { SwitchControl } from "./SwitchControl";

export type FormFieldProps = {
  label: string;
  name: string;
  type: string;
  options?: Array<OptionType>;
  tooltip?: string;
  group?: string;
  isRequired?: boolean;
  isDisabled?: boolean;
  isClearable?: boolean;
  placeholder?: string;
  noOptionsMessage?: ((obj: { inputValue: string }) => ReactNode) | undefined;
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
            {field.label} {field.isRequired && "*"}
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
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            inputProps={{
              type: "number",
              autoFocus,
              placeholder: field.placeholder
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
    case "multiSelect":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <MultiSelectControl
            isDisabled={field.isDisabled}
            selectProps={{
              autoFocus,
              options: field.options,
              noOptionsMessage: field.noOptionsMessage,
              placeholder: field.placeholder
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
    case "creatableSingleSelect":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <CreatableSingleSelectControl
            isDisabled={field.isDisabled}
            selectProps={{
              isClearable: field.isClearable,
              autoFocus,
              options: field.options,
              noOptionsMessage: field.noOptionsMessage,
              placeholder: field.placeholder
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
    case "creatableMultiSelect":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <CreatableMultiSelectControl
            isDisabled={field.isDisabled}
            selectProps={{
              isClearable: field.isClearable,
              autoFocus,
              options: field.options,
              noOptionsMessage: field.noOptionsMessage,
              placeholder: field.placeholder
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
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
            inputProps={{ autoFocus, placeholder: field.placeholder }}
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
