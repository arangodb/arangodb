import { FormLabel, Input } from "@chakra-ui/react";
import { get } from "lodash";
import React from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { FormDispatch, FormProps } from "../../../utils/constants";
import { getNumericFieldSetter } from "../../../utils/helpers";
import { BytesAccumConsolidationPolicy, FormState } from "../constants";

type BytesAccumConsolidationPolicyFormProps = Omit<
  FormProps<BytesAccumConsolidationPolicy>,
  "dispatch"
> & {
  dispatch: FormDispatch<FormState>;
};
export const BytesAccumConsolidationPolicyForm = ({
  formState,
  dispatch,
  disabled
}: BytesAccumConsolidationPolicyFormProps) => {
  const threshold = get(formState, ["consolidationPolicy", "threshold"], "");

  return (
    <>
      <FormLabel htmlFor="threshold"> Threshold:</FormLabel>
      <Input
        id="threshold"
        type={"number"}
        value={threshold}
        isDisabled={disabled}
        min={0.0}
        max={1.0}
        step={0.0001}
        onChange={getNumericFieldSetter(
          "consolidationPolicy.threshold",
          dispatch
        )}
      />
      <InfoTooltip label="Consolidation is performed on segments which accumulated size in bytes is less than all segments’ byte size multiplied by the threshold." />
    </>
  );
};
