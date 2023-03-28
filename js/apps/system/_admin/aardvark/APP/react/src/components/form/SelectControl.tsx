import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps, PropsValue } from "react-select";
import SelectBase from "../select/SelectBase";
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
  const value = selectProps?.options?.find(option => {
    return (option as OptionType).value === field.value;
  }) as PropsValue<OptionType>;
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <SelectBase
        {...field}
        value={value}
        inputId={name}
        isDisabled={isSubmitting}
        {...selectProps}
        onChange={value => {
          helper.setValue((value as OptionType)?.value);
        }}
      />
    </FormikFormControl>
  );
};
