import React from "react";
import SplitPane from "react-split-pane";
import { FormProps, State } from "../../utils/constants";
import { FormState } from "./constants";
import useElementSize from "./useElementSize";
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
} & Pick<FormProps<FormState>, 'dispatch'>) => {
  const [sectionRef, sectionSize] = useElementSize();
  const sectionWidth = sectionSize.width;
  const maxSize = sectionWidth - 200;
  const localStorageSplitPos = localStorage.getItem("splitPos") || "400";
  const splitPos = parseInt(localStorageSplitPos, 10);
  return (
    <section ref={sectionRef}>
      <SplitPane
        paneStyle={{ overflow: "scroll" }}
        maxSize={maxSize}
        defaultSize={splitPos}
        onChange={size => localStorage.setItem("splitPos", size.toString())}
        style={{
          paddingTop: "15px",
          marginTop: "10px",
          marginLeft: "15px",
          marginRight: "15px"
        }}
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
      </SplitPane>
    </section>
  );
};
