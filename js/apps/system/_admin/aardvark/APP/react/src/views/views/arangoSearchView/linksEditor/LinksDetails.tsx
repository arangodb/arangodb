import { Checkbox, Grid, HStack, Stack } from "@chakra-ui/react";

import { useFormikContext } from "formik";
import { get } from "lodash";
import React from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useEditViewContext } from "../../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../../View.types";
import { AnalyzersDropdown } from "./AnalyzersDropdown";
import { FieldsDropdown } from "./FieldsDropdown";
import { LinksBreadCrumb } from "./LinksBreadCrumb";
import { useFieldPath, useLinkModifiers } from "./useLinkModifiers";

export const LinksDetails = () => {
  const { currentLink, currentField } = useEditViewContext();
  const { values } = useFormikContext<ArangoSearchViewPropertiesType>();
  const { fieldPath } = useFieldPath();
  const hasValue = currentLink
    ? get(values.links?.[currentLink], fieldPath)
    : false;
  if (
    !currentLink ||
    !values.links?.[currentLink] ||
    (fieldPath.length > 0 && !hasValue)
  ) {
    return null;
  }
  const isFieldView = currentField.length > 0;

  return (
    <Stack backgroundColor="gray.100">
      <LinksBreadCrumb />
      <Grid templateColumns="minmax(300px, 1fr) 1fr" columnGap="10">
        <Stack padding="2">
          <AnalyzersDropdown />
          <FieldsDropdown />
        </Stack>
        <Stack padding="2">
          <CheckboxField
            id="includeAllFields"
            label="Include All Fields"
            tooltip="Process all document attributes."
          />
          <CheckboxField
            id="trackListPositions"
            label="Track List Positions"
            tooltip="For array values track the value position in arrays."
          />
          <StoreIDValuesCheckbox />
          <CheckboxField
            id="cache"
            label="Cache"
            isDisabled={!window.frontendConfig.isEnterprise}
            tooltip={
              window.frontendConfig.isEnterprise
              ? "Always cache field normalization values in memory."
              : "Field normalization value caching is available in Enterprise plans."
            }
          />
          {!isFieldView && (
            <CheckboxField
              id="inBackground"
              label="In Background"
              tooltip="If selected, no exclusive lock is used on the source collection during View index creation."
            />
          )}
        </Stack>
      </Grid>
    </Stack>
  );
};

const StoreIDValuesCheckbox = () => {
  const id = "storeValues";
  const { setCurrentLinkValue, getCurrentLinkValueDefault } = useLinkModifiers();
  const isChecked = getCurrentLinkValueDefault([id]) === "id" ? true : false;
  return (
    <HStack>
      <Checkbox
        isChecked={isChecked}
        margin="0"
        borderColor="gray.400"
        onChange={() => {
          const newChecked = !isChecked;
          setCurrentLinkValue({ id: [id], value: newChecked ? "id" : "none" });
        }}
      >
        Store ID Values
      </Checkbox>
      <InfoTooltip label="Store information about value presence to allow use of the EXISTS() function." />
    </HStack>
  );
};

const CheckboxField = ({
  id,
  label,
  tooltip,
  isDisabled,
  onChange
}: {
  id: string;
  label: string;
  tooltip: string;
  isDisabled?: boolean;
  onChange?: (value: boolean) => void;
}) => {
  const { setCurrentLinkValue, getCurrentLinkValueDefault } = useLinkModifiers();
  const isChecked = getCurrentLinkValueDefault([id]);
  return (
    <HStack>
      <Checkbox
        isDisabled={isDisabled}
        isChecked={isChecked}
        margin="0"
        borderColor="gray.400"
        onChange={() => {
          const newValue = !isChecked;
          if (onChange) {
            onChange(newValue);
          }
          setCurrentLinkValue({
            id: [id],
            value: newValue
          });
        }}
      >
        {label}
      </Checkbox>
      <InfoTooltip label={tooltip} />
    </HStack>
  );
};
