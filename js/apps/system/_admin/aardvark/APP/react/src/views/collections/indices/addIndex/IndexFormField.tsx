import { InfoIcon } from "@chakra-ui/icons";
import { FormLabel, Tooltip } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";

export type IndexFormFieldProps = {
  label: string;
  name: string;
  type: string;
  tooltip?: string;
  isRequired?: boolean;
  isDisabled?: boolean;
};

export const IndexFormField = ({
  field,
  index
}: {
  field: IndexFormFieldProps;
  index: number;
}) => {
  switch (field.type) {
    case "boolean":
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <SwitchControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip && <InfoTooltip label={field.tooltip} />}
        </>
      );
    case "number":
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            inputProps={{ type: "number", autoFocus: index === 0 }}
            isRequired={field.isRequired}
            name={field.name}
          />
          {field.tooltip && <InfoTooltip label={field.tooltip} />}
        </>
      );
    case "text":
    default:
      return (
        <>
          <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
          <InputControl
            isDisabled={field.isDisabled}
            isRequired={field.isRequired}
            name={field.name}
            inputProps={{ autoFocus: index === 0 }}
          />
          {field.tooltip && <InfoTooltip label={field.tooltip} />}
        </>
      );
  }
};

const InfoTooltip = ({ label }: { label: string }) => {
  return (
    <Tooltip hasArrow label={label} placement="top">
      <InfoIcon position="relative" top="3" />
    </Tooltip>
  );
};
