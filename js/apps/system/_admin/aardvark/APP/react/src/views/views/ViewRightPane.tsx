import { Box } from "@chakra-ui/react";
import React from "react";
import { FormDispatch, State } from "../../utils/constants";
import { FormState } from "./constants";
import JsonForm from "./forms/JsonForm";

export const ViewRightPane = ({
  formState,
  dispatch,
  state
}: {
  formState: FormState;
  dispatch: FormDispatch<FormState>;
  state: State<FormState>;
}) => {
  return (
    <Box minWidth="0" paddingY="4" height="full">
      <JsonForm
        formState={formState}
        dispatch={dispatch}
        renderKey={state.renderKey}
        mode={"code"}
      />
    </Box>
  );
};
