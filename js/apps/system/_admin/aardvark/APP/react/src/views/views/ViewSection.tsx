import { Box } from "@chakra-ui/react";
import React from "react";
import Split from "react-split";
import { FormProps, State } from "../../utils/constants";
import { FormState } from "./constants";
import { ViewLeftPane } from "./ViewLeftPane";
import { ViewRightPane } from "./ViewRightPane";

export const ViewSection = ({
  name,
  formState,
  dispatch,
  isAdminUser,
  state
}: {
  name: string;
  formState: FormState;
  isAdminUser: boolean;
  state: State<FormState>;
} & Pick<FormProps<FormState>, "dispatch">) => {
  const localStorageSplitSize = localStorage.getItem("viewJSONSplitSizes");
  let sizes = [50, 50];
  try {
    sizes = localStorageSplitSize ? JSON.parse(localStorageSplitSize) : sizes;
  } catch {
    // ignore error
  }
  return (
    <Box as="section" width="full" height="calc(100vh - 200px)">
      <Split
        style={{
          display: "flex",
          height: "100%"
        }}
        sizes={sizes}
        onDragEnd={sizes =>
          localStorage.setItem("viewJSONSplitSizes", JSON.stringify(sizes))
        }
      >
        <ViewLeftPane
          name={name}
          formState={formState}
          dispatch={dispatch}
          isAdminUser={isAdminUser}
        />
        <ViewRightPane
          formState={formState}
          dispatch={dispatch}
          state={state}
        />
      </Split>
    </Box>
  );
};
