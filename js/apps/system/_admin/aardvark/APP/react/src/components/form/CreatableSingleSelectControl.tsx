import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps, PropsValue } from "react-select";
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
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();

  let value = selectProps?.options?.find(option => {
    return (option as OptionType).value === field.value;
  }) as PropsValue<OptionType>;

  // this is when a value is newly created
  if (!selectProps?.options && field.value) {
    value = {
      value: field.value,
      label: field.value
    }
  }
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
