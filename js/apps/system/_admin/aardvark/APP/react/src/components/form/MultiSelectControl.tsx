import { useField, useFormikContext } from "formik";
import React from "react";
import {
  MultiValue,
  Props as ReactSelectProps,
  PropsValue
} from "react-select";
import SelectBase from "../select/SelectBase";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType>;
};

export const MultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();

  const value = selectProps?.options?.filter(option => {
    return field.value.includes((option as OptionType).value);
  }) as PropsValue<OptionType>;

  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <SelectBase
        {...field}
        isMulti
        value={value}
        inputId={name}
        isDisabled={isSubmitting}
        {...selectProps}
        onChange={values => {
          const valueStringArray = (values as MultiValue<OptionType>)?.map(
            value => {
              return value.value;
            }
          );
          helper.setValue(valueStringArray);
        }}
      />
    </FormikFormControl>
  );
};
