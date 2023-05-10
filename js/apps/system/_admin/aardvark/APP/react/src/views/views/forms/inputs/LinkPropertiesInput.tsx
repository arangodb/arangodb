import {
  Checkbox,
  FormControl,
  FormLabel,
  Grid,
  HStack,
  Stack
} from "@chakra-ui/react";
import { get } from "lodash";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { FormProps } from "../../../../utils/constants";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import { LinkProperties } from "../../constants";
import { AnalyzersDropdown } from "./AnalyzersDropdown";
import { FieldsDropdown } from "./FieldsDropdown";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({
  formState,
  dispatch,
  disabled,
  basePath
}: LinkPropertiesInputProps) => {
  const updateStoreValues = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: "setField",
      field: {
        path: "storeValues",
        value: event.target.checked ? "id" : "none"
      },
      basePath
    });
  };

  const storeIdValues = get(formState, "storeValues") === "id";
  const hideInBackgroundField = disabled || basePath.includes(".fields");

  return (
    <Grid
      templateColumns="minmax(300px, 1fr) 1fr"
      columnGap="10"
      border="2px solid"
      borderColor="gray.300"
      paddingX="4"
      paddingY="2"
      background="gray.200"
    >
      <Stack spacing="4">
        <Stack>
          <FormLabel margin="0">Analyzers</FormLabel>
          <AnalyzersDropdown
            basePath={basePath}
            isDisabled={!!disabled}
            formState={formState}
          />
        </Stack>

        <Stack>
          <HStack alignItems="center">
            <FormLabel margin="0">Fields</FormLabel>
            <InfoTooltip label="Add field names that you want to be indexed here. Click on a field name to set field details." />
          </HStack>
          <FieldsDropdown
            basePath={basePath}
            isDisabled={!!disabled}
            formState={formState}
          />
        </Stack>
      </Stack>
      <Stack>
        <HStack alignItems="center">
          <FormControl width="auto">
            <HStack>
              <Checkbox
                borderColor="gray.400"
                id="includeAllFields"
                onChange={getBooleanFieldSetter(
                  "includeAllFields",
                  dispatch,
                  basePath
                )}
                disabled={disabled}
                isChecked={!!formState.includeAllFields}
              />
              <FormLabel htmlFor="includeAllFields">
                Include All Fields
              </FormLabel>
            </HStack>
          </FormControl>
          <InfoTooltip label="Process all document attributes." />
        </HStack>
        <HStack alignItems="center">
          <FormControl width="auto">
            <HStack>
              <Checkbox
                borderColor="gray.400"
                id="trackListPositions"
                onChange={getBooleanFieldSetter(
                  "trackListPositions",
                  dispatch,
                  basePath
                )}
                disabled={disabled}
                isChecked={!!formState.trackListPositions}
              />
              <FormLabel htmlFor="trackListPositions">
                Track List Positions
              </FormLabel>
            </HStack>
          </FormControl>
          <InfoTooltip label="For array values track the value position in arrays." />
        </HStack>
        <HStack alignItems="center">
          <FormControl width="auto">
            <HStack>
              <Checkbox
                borderColor="gray.400"
                id="storeIdValue"
                onChange={updateStoreValues}
                disabled={disabled}
                isChecked={!!storeIdValues}
              />
              <FormLabel htmlFor="storeIdValue">Store ID Values</FormLabel>
            </HStack>
          </FormControl>
          <InfoTooltip label="Store information about value presence to allow use of the EXISTS() function." />
        </HStack>
        {hideInBackgroundField ? null : (
          <HStack alignItems="center">
            <FormControl width="auto">
              <HStack>
                <Checkbox
                  borderColor="gray.400"
                  id="inBackground"
                  onChange={getBooleanFieldSetter(
                    "inBackground",
                    dispatch,
                    basePath
                  )}
                  disabled={disabled}
                  isChecked={!!formState.inBackground}
                />
                <FormLabel htmlFor="inBackground">In Background</FormLabel>
              </HStack>
            </FormControl>
            <InfoTooltip label="If selected, no exclusive lock is used on the source collection during View index creation." />
          </HStack>
        )}
      </Stack>
    </Grid>
  );
};

export default LinkPropertiesInput;
