import { Box } from "@chakra-ui/react";
import React from "react";
import { Split } from "../../components/split/Split";
import { FormProps, State } from "../../utils/constants";
import { FormState } from "./constants";
import { ViewLeftPane } from "./ViewLeftPane";
import { ViewRightPane } from "./ViewRightPane";

export const ViewSection = ({
  formState,
  dispatch,
  isAdminUser,
  state
}: {
  formState: FormState;
  isAdminUser: boolean;
  state: State<FormState>;
} & Pick<FormProps<FormState>, "dispatch">) => {
  return (
    <Box as="section" width="full" height="calc(100vh - 200px)">
      <Split
        storageKey={"viewJSONSplitTemplate"}
        render={({ getGridProps, getGutterProps }) => {
          const gridProps = getGridProps();
          const gutterProps = getGutterProps("column", 1);
          return (
            <Box display="grid" height="full" {...gridProps}>
              <ViewLeftPane
                formState={formState}
                dispatch={dispatch}
                isAdminUser={isAdminUser}
              />
              <Box
                gridRow="1/-1"
                backgroundColor="gray.300"
                cursor="col-resize"
                gridColumn="2"
                position="relative"
                {...gutterProps}
              ></Box>
              <ViewRightPane
                formState={formState}
                dispatch={dispatch}
                state={state}
              />
            </Box>
          );
        }}
      />
    </Box>
  );
};
