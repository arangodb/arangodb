import { Box, FormLabel, Select } from "@chakra-ui/react";
import { get } from "lodash";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { FormDispatch, FormProps } from "../../../utils/constants";
import {
  BytesAccumConsolidationPolicy,
  FormState,
  TierConsolidationPolicy,
  ViewProperties
} from "../constants";
import { BytesAccumConsolidationPolicyForm } from "./BytesAccumConsolidationPolicyForm";
import { TierConsolidationPolicyForm } from "./TierConsolidationPolicyForm";

type ConsolidationPolicyFormProps = Omit<
  FormProps<ViewProperties>,
  "dispatch"
> & {
  dispatch: FormDispatch<FormState>;
};
const ConsolidationPolicyForm = ({
  formState,
  dispatch,
  disabled
}: ConsolidationPolicyFormProps) => {
  const updateConsolidationPolicyType = (
    event: ChangeEvent<HTMLSelectElement>
  ) => {
    dispatch({
      type: "setField",
      field: {
        path: "consolidationPolicy.type",
        value: event.target.value
      }
    });
  };

  const policyType = get(formState, ["consolidationPolicy", "type"], "tier");

  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      alignItems="center"
    >
      <FormLabel htmlFor="policyType">Policy Type:</FormLabel>
      <Select
        background={"white"}
        isDisabled={disabled}
        value={policyType}
        onChange={updateConsolidationPolicyType}
      >
        <option key={"tier"} value={"tier"}>
          Tier
        </option>
        <option disabled key={"bytes_accum"} value={"bytes_accum"}>
          Bytes Accum [DEPRECATED]
        </option>
      </Select>
      <InfoTooltip label="Represents the type of policy." />

      {policyType === "tier" ? (
        <TierConsolidationPolicyForm
          formState={formState as TierConsolidationPolicy}
          dispatch={dispatch}
          disabled={disabled}
        />
      ) : (
        <BytesAccumConsolidationPolicyForm
          formState={formState as BytesAccumConsolidationPolicy}
          dispatch={dispatch}
          disabled={disabled}
        />
      )}
    </Box>
  );
};

export default ConsolidationPolicyForm;
