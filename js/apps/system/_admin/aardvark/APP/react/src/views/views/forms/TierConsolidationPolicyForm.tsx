import { FormLabel, Input } from "@chakra-ui/react";
import { get } from "lodash";
import React from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { FormDispatch, FormProps } from "../../../utils/constants";
import { getNumericFieldSetter } from "../../../utils/helpers";
import { FormState, TierConsolidationPolicy } from "../constants";

type TierConsolidationPolicyFormProps = Omit<
  FormProps<TierConsolidationPolicy>,
  "dispatch"
> & {
  dispatch: FormDispatch<FormState>;
};
export const TierConsolidationPolicyForm = ({
  formState,
  dispatch,
  disabled
}: TierConsolidationPolicyFormProps) => {
  const segmentsMin = get(
    formState,
    ["consolidationPolicy", "segmentsMin"],
    ""
  );
  const segmentsMax = get(
    formState,
    ["consolidationPolicy", "segmentsMax"],
    ""
  );
  const segmentsBytesMax = get(
    formState,
    ["consolidationPolicy", "segmentsBytesMax"],
    ""
  );
  const segmentsBytesFloor = get(
    formState,
    ["consolidationPolicy", "segmentsBytesFloor"],
    ""
  );

  return (
    <>
      <>
        <FormLabel htmlFor="segmentsMin">Segments Min:</FormLabel>
        <Input
          id="segmentsMin"
          type={"number"}
          value={segmentsMin}
          isDisabled={disabled}
          min={0}
          step={1}
          onChange={getNumericFieldSetter(
            "consolidationPolicy.segmentsMin",
            dispatch
          )}
        />
        <InfoTooltip label="The minimum number of segments that will be evaluated as candidates for consolidation." />
      </>
      <>
        <FormLabel htmlFor="segmentsMax">Segments Max:</FormLabel>
        <Input
          id="segmentsMax"
          type={"number"}
          value={segmentsMax}
          isDisabled={disabled}
          min={0}
          step={1}
          onChange={getNumericFieldSetter(
            "consolidationPolicy.segmentsMax",
            dispatch
          )}
        />
        <InfoTooltip label="The maximum number of segments that will be evaluated as candidates for consolidation." />
      </>
      <>
        <FormLabel htmlFor="segmentsBytesMax">Segments Bytes Max:</FormLabel>
        <Input
          id="segmentsBytesMax"
          type={"number"}
          value={segmentsBytesMax}
          isDisabled={disabled}
          min={0}
          step={1}
          onChange={getNumericFieldSetter(
            "consolidationPolicy.segmentsBytesMax",
            dispatch
          )}
        />
        <InfoTooltip label="Maximum allowed size of all consolidated segments in bytes." />
      </>
      <>
        <FormLabel htmlFor="segmentsBytesFloor">
          Segments Bytes Floor:
        </FormLabel>
        <Input
          id="segmentsBytesFloor"
          type={"number"}
          value={segmentsBytesFloor}
          isDisabled={disabled}
          min={0}
          step={1}
          onChange={getNumericFieldSetter(
            "consolidationPolicy.segmentsBytesFloor",
            dispatch
          )}
        />
        <InfoTooltip label="Defines the value (in bytes) to treat all smaller segments as equal for consolidation selection." />
      </>
    </>
  );
};
