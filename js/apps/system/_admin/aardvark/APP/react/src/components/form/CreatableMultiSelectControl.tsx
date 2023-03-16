import { useField, useFormikContext } from "formik";
import React from "react";
import {
  MultiValue,
  Props as ReactSelectProps,
  PropsValue
} from "react-select";
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

const CreatableSelectBase = getSelectBase(CreatableSelect);

export const CreatableMultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();

  let value = selectProps?.options?.filter(option => {
    return (option as OptionType).value === field.value;
  }) as PropsValue<OptionType>;

  // this is when a value is newly created
  if (!selectProps?.options && field.value) {
    value = field.value.map((value: any) => {
      return {
        value,
        label: value
      };
    });
  }
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <CreatableSelectBase
        {...field}
        isMulti
        value={value}
        inputId={name}
        isDisabled={isSubmitting}
        onChange={values => {
          const valueStringArray = (values as MultiValue<OptionType>)?.map(
            value => {
              return value.value;
            }
          );
          helper.setValue(valueStringArray);
        }}
        {...selectProps}
      />
    </FormikFormControl>
  );
};
