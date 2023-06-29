import { useField, useFormikContext } from "formik";
import React from "react";
import { Props as ReactSelectProps, PropsValue } from "react-select";
import CreatableMultiSelect from "../select/CreatableMultiSelect";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType, true>;
};

export const CreatableMultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();

  let value =
    typeof field.value === "string"
      ? (selectProps?.options?.filter(option => {
          return (option as OptionType).value === field.value;
        }) as PropsValue<OptionType>)
      : [];

  // this is when a value is newly created
  if ((!selectProps?.options && field.value) || Array.isArray(field.value)) {
    value = field.value.map((value: any) => {
      return {
        value,
        label: value
      };
    });
  }
  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <CreatableMultiSelect
        {...field}
        value={value}
        inputId={name}
        isDisabled={rest.isDisabled || isSubmitting}
        {...selectProps}
        onChange={values => {
          const valueStringArray = values.map(value => {
            return value.value;
          });
          helper.setValue(valueStringArray);
        }}
      />
    </FormikFormControl>
  );
};
