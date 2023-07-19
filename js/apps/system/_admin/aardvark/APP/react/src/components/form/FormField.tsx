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
  const selectProps = {
    autoFocus,
    placeholder: field.placeholder,
    isClearable: field.isClearable,
    options: field.options,
    noOptionsMessage: field.noOptionsMessage
  };
  const inputProps = {
    autoFocus,
    placeholder: field.placeholder,
    type: field.type
  };

  const commonProps = {
    isDisabled: field.isDisabled,
    isRequired: field.isRequired,
    name: field.name
  };

  switch (field.type) {
    case "boolean":
      return (
        <>
          <FormLabel margin="0" htmlFor={field.name}>
            {field.label} {field.isRequired && "*"}
          </FormLabel>
          <SwitchControl {...commonProps} />
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
          <InputControl {...commonProps} inputProps={inputProps} />
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
          <MultiSelectControl {...commonProps} selectProps={selectProps} />
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
            {...commonProps}
            selectProps={selectProps}
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
            {...commonProps}
            selectProps={selectProps}
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
          <InputControl {...commonProps} inputProps={inputProps} />
          {field.tooltip ? (
            <IndexInfoTooltip label={field.tooltip} />
          ) : (
            <Spacer />
          )}
        </>
      );
  }
};
