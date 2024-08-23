import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps, PropsValue } from "react-select";
import SingleSelect from "../select/SingleSelect";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType, false>;
};

export const SelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();
  const value =
    (selectProps?.options?.find(option => {
      return (option as OptionType).value === field.value;
    }) as PropsValue<OptionType>) || null;
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <SingleSelect
        {...field}
        isDisabled={rest.isDisabled || isSubmitting}
        value={value}
        inputId={name}
        {...selectProps}
        onChange={value => {
          helper.setValue(value?.value);
        }}
      />
    </FormikFormControl>
  );
};
