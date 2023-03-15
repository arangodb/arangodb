import { Box, FormLabel } from "@chakra-ui/react";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { OptionType } from "../../../../components/select/SelectBase";
import { IndexFormFieldProps } from "./IndexFormField";
import { InfoTooltip } from "./InfoTooltip";
import { useInvertedIndexContext } from "./InvertedIndexContext";

const MultiValueLabel = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentFieldPath } = useInvertedIndexContext();
  return (
    <Box
      onClick={() => setCurrentFieldPath(props.data.value)}
      textDecoration="underline"
      cursor="pointer"
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};

export const InvertedIndexFieldsDropdown = ({
  field,
  autoFocus
}: {
  autoFocus: boolean;
  field: IndexFormFieldProps;
}) => {
  return (
    <>
      <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
      <CreatableMultiSelectControl
        isDisabled={field.isDisabled}
        selectProps={{
          autoFocus,
          components: {
            MultiValueLabel
          }
        }}
        isRequired={field.isRequired}
        name={field.name}
      />
      {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
    </>
  );
};
