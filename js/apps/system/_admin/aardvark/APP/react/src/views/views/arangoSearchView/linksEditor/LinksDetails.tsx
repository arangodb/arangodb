import { Checkbox, Grid, HStack, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import { cloneDeep, get, set } from "lodash";
import React from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useEditViewContext } from "../../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../../searchView.types";
import { AnalyzersDropdown } from "./AnalyzersDropdown";
import { FieldsDropdown } from "./FieldsDropdown";
import { LinksBreadCrumb } from "./LinksBreadCrumb";
import { useFieldPath } from "./useLinksSetter";

export const LinksDetails = () => {
  const { currentLink, currentField } = useEditViewContext();
  if (!currentLink) {
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
  const label = "Store ID Values";
  const tooltip =
    "Store information about value presence to allow use of the EXISTS() function.";
  const { currentLink, currentField } = useEditViewContext();
  const { fieldPath } = useFieldPath();
  const { values, setValues } =
    useFormikContext<ArangoSearchViewPropertiesType>();

  const isFieldView = currentField.length > 0;
  const currentLinkFieldsValue = currentLink
    ? values?.links?.[currentLink]
    : {};
  const isChecked = isFieldView
    ? get(currentLinkFieldsValue, [...fieldPath, id]) === "id" || false
    : get(currentLinkFieldsValue, id) === "id" || false;
  return (
    <HStack>
      <Checkbox
        isChecked={isChecked}
        margin="0"
        borderColor="gray.400"
        onChange={() => {
          const newValue = !isChecked;
          if (!currentLinkFieldsValue || !currentLink) {
            return;
          }
          if (isFieldView) {
            const newLink = set(
              cloneDeep(currentLinkFieldsValue),
              [...fieldPath, id],
              newValue ? "id" : "none"
            );
            setValues({
              ...values,
              links: {
                ...values.links,
                [currentLink]: newLink
              }
            });
          } else {
            const newLink = set(
              cloneDeep(currentLinkFieldsValue),
              id,
              newValue ? "id" : "none"
            );
            setValues({
              ...values,
              links: {
                ...values.links,
                [currentLink]: newLink
              }
            });
          }
        }}
      >
        {label}
      </Checkbox>
      <InfoTooltip label={tooltip} />
    </HStack>
  );
};

const CheckboxField = ({
  id,
  label,
  tooltip,
  onChange
}: {
  id: string;
  label: string;
  tooltip: string;
  onChange?: (value: boolean) => void;
}) => {
  const { currentLink, currentField } = useEditViewContext();
  const { fieldPath } = useFieldPath();
  const { values, setValues } =
    useFormikContext<ArangoSearchViewPropertiesType>();

  const isFieldView = currentField.length > 0;
  const currentLinkFieldsValue = currentLink
    ? values?.links?.[currentLink]
    : {};
  const isChecked = isFieldView
    ? get(currentLinkFieldsValue, [...fieldPath, id]) || false
    : get(currentLinkFieldsValue, id) || false;
  return (
    <HStack>
      <Checkbox
        isChecked={isChecked}
        margin="0"
        borderColor="gray.400"
        onChange={() => {
          const newValue = !isChecked;
          if (onChange) {
            onChange(newValue);
          }
          if (!currentLinkFieldsValue || !currentLink) {
            return;
          }
          if (isFieldView) {
            const newLink = set(
              cloneDeep(currentLinkFieldsValue),
              [...fieldPath, id],
              newValue
            );
            setValues({
              ...values,
              links: {
                ...values.links,
                [currentLink]: newLink
              }
            });
          } else {
            const newLink = set(
              cloneDeep(currentLinkFieldsValue),
              id,
              newValue
            );
            setValues({
              ...values,
              links: {
                ...values.links,
                [currentLink]: newLink
              }
            });
          }
        }}
      >
        {label}
      </Checkbox>
      <InfoTooltip label={tooltip} />
    </HStack>
  );
};
