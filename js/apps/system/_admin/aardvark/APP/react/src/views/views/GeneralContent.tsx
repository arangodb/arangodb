import { Box, FormLabel, Input } from "@chakra-ui/react";
import React from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { FormDispatch } from "../../utils/constants";
import {
  getNumericFieldSetter,
  getNumericFieldValue
} from "../../utils/helpers";
import { FormState } from "./constants";

export const GeneralContent = ({
  formState,
  dispatch,
  isAdminUser
}: {
  formState: FormState;
  dispatch: FormDispatch<FormState>;
  isAdminUser: boolean;
}) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      alignItems="center"
    >
      <>
        <FormLabel htmlFor="writebufferIdle">Writebuffer Idle</FormLabel>
        <Input
          type="number"
          id="writebufferIdle"
          value={getNumericFieldValue(formState.writebufferIdle)}
          isDisabled
        />
        <InfoTooltip label="Maximum number of writers (segments) cached in the pool (default: 64, use 0 to disable, immutable)" />
      </>
      <>
        <FormLabel htmlFor="writebufferActive">Write Buffer Active</FormLabel>
        <Input
          type="number"
          id="writebufferActive"
          value={getNumericFieldValue(formState.writebufferActive)}
          isDisabled
        />
        <InfoTooltip label="Maximum number of concurrent active writers (segments) that perform a transaction. Other writers (segments) wait till current active writers (segments) finish (default: 0, use 0 to disable, immutable)." />
      </>
      <>
        <FormLabel htmlFor="writebufferSizeMax">Writebuffer Size Max</FormLabel>
        <Input
          type="number"
          id="writebufferSizeMax"
          value={getNumericFieldValue(formState.writebufferSizeMax)}
          isDisabled
        />
        <InfoTooltip label="Maximum memory byte size per writer (segment) before a writer (segment) flush is triggered. 0 value turns off this limit for any writer (buffer) and data will be flushed periodically based on the value defined for the flush thread (ArangoDB server startup option). 0 value should be used carefully due to high potential memory consumption (default: 33554432, use 0 to disable, immutable)." />
      </>
      <>
        <FormLabel htmlFor="cleanupIntervalStep">
          Cleanup Interval Step:
        </FormLabel>
        <Input
          id="cleanupIntervalStep"
          type={"number"}
          isDisabled={!isAdminUser}
          min={0}
          step={1}
          value={getNumericFieldValue(formState.cleanupIntervalStep)}
          onChange={getNumericFieldSetter("cleanupIntervalStep", dispatch)}
        />
        <InfoTooltip
          label={`ArangoSearch waits at least this many commits between removing unused files in its data directory.`}
        />
      </>
      <>
        <FormLabel htmlFor="commitIntervalMsec">
          Commit Interval (msec):
        </FormLabel>
        <Input
          id="commitIntervalMsec"
          type={"number"}
          isDisabled={!isAdminUser}
          min={0}
          step={1}
          value={getNumericFieldValue(formState.commitIntervalMsec)}
          onChange={getNumericFieldSetter("commitIntervalMsec", dispatch)}
        />
        <InfoTooltip label="Wait at least this many milliseconds between committing View data store changes and making documents visible to queries." />
      </>
      <>
        <FormLabel htmlFor="consolidationIntervalMsec">
          Consolidation Interval (msec):
        </FormLabel>
        <Input
          id="consolidationIntervalMsec"
          type={"number"}
          isDisabled={!isAdminUser}
          min={0}
          step={1}
          value={getNumericFieldValue(formState.consolidationIntervalMsec)}
          onChange={getNumericFieldSetter(
            "consolidationIntervalMsec",
            dispatch
          )}
        />
        <InfoTooltip label="Wait at least this many milliseconds between index segments consolidations." />
      </>
    </Box>
  );
};
