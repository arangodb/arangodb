import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps } from "react-select";
import CreatableSingleSelect from "../select/CreatableSingleSelect";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType, false>;
};

export const CreatableSingleSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField<string | undefined>(name);
  const { isSubmitting } = useFormikContext();
  const value = getValue({
    field,
    options: selectProps?.options as OptionType[]
  });
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <CreatableSingleSelect
        {...field}
        value={value}
        inputId={name}
        isDisabled={rest.isDisabled || isSubmitting}
        {...selectProps}
        onChange={selectedOption => {
          helper.setValue(selectedOption?.value);
        }}
      />
    </FormikFormControl>
  );
};
const createValue = (value: string) => {
  return {
    value,
    label: value
  };
};
function getValue({
  field,
  options
}: {
  field: { value: string | undefined };
  options: OptionType[] | undefined;
}) {
  if (!field.value) {
    return undefined;
  }

  // if no options are provided, it's a newly created value
  if (!options || options.length === 0) {
    return createValue(field.value);
  }
  if (typeof field.value === "string") {
    // if it's a string, find it in options
    const foundValue = options.find(option => {
      return option.value === field.value;
    });
    if (foundValue) {
      return foundValue;
    } else {
      // if not found, it's a newly created value
      return createValue(field.value);
    }
  }
  return field.value;
}
