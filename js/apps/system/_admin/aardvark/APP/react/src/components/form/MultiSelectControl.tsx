import { useField, useFormikContext } from "formik";
import React from "react";
import Select, { Props as ReactSelectProps, PropsValue } from "react-select";
import { getSelectBase } from "../select/SelectBase";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

type OptionType = {
  label: string;
  value: string;
};
export type InputControlProps = BaseFormControlProps & {
  selectProps?: ReactSelectProps<OptionType, true>;
};
const SelectBase = getSelectBase<true>(Select);
export const MultiSelectControl = (props: InputControlProps) => {
  const { name, label, selectProps, ...rest } = props;
  const [field, , helper] = useField(name);
  const { isSubmitting } = useFormikContext();

  const value = selectProps?.options?.filter(option => {
    return field.value?.includes((option as OptionType).value);
  }) as PropsValue<OptionType>;

  return (
    <FormikFormControl name={name} label={label} {...rest}>
      <SelectBase
        {...field}
        value={value}
        inputId={name}
        isDisabled={rest.isDisabled || isSubmitting}
        onChange={values => {
          const valueStringArray = values?.map(value => {
            return value.value;
          });
          helper.setValue(valueStringArray);
        }}
        {...selectProps}
        isMulti
      />
    </FormikFormControl>
  );
};
