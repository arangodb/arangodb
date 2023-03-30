import { useField, useFormikContext } from "formik";
import React from "react";
import { MultiValue, Props as ReactSelectProps, PropsValue } from "react-select";
import CreatableSelect from "react-select/creatable";
import { getSelectBase } from "../select/SelectBase";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType>;
};

const CreatableSelectBase = getSelectBase<true>(CreatableSelect);

export const CreatableMultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();
  const value = selectProps?.options?.find(option => {
    return (option as OptionType).value === field.value;
  }) as PropsValue<OptionType>;
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <CreatableSelectBase
        {...field}
        value={value}
        inputId={name}
        isDisabled={isSubmitting}
        {...selectProps}
        isMulti
        onChange={values => {
          const valueStringArray = (values as MultiValue<OptionType>)?.map(value => {
            return value.value
          });
          helper.setValue(valueStringArray);
        }}
      />
    </FormikFormControl>
  );
};

