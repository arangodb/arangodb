import { Box, Flex, Switch, SwitchProps } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React from "react";
import { InfoTooltip } from "../tooltip/InfoTooltip";
import { BaseFormControlProps, FormikFormControl } from "./FormikFormControl";

export type InputControlProps = BaseFormControlProps & {
  switchProps?: SwitchProps;
};

export const SwitchControl = (props: InputControlProps) => {
  const { name, label, switchProps, tooltip, ...rest } = props;
  const [field] = useField(name);
  const { isSubmitting } = useFormikContext();
  return (
    <Flex alignItems="end">
      <Box>
        <FormikFormControl
          name={name}
          label={label}
          isDisabled={isSubmitting}
          {...rest}
        >
          <Switch
            {...field}
            isChecked={!!field.value}
            id={name}
            {...switchProps}
          />
        </FormikFormControl>
      </Box>
      {tooltip && <InfoTooltip label={tooltip} />}

    </Flex>
  );
};
