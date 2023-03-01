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
} & Pick<FormProps<FormState>, "dispatch">) => {
  const [sectionRef, sectionSize] = useElementSize();
  const sectionWidth = sectionSize.width;
  const maxSize = sectionWidth - 200;
  const localStorageSplitPos = localStorage.getItem("splitPos") || "400";
  let splitPos = parseInt(localStorageSplitPos, 10);
  if (splitPos > (sectionWidth - 200)) {
    splitPos = sectionWidth - 200;
  }
  return (
    <section ref={sectionRef}>
      <SplitPane
        paneStyle={{ overflow: "auto" }}
        maxSize={maxSize}
        defaultSize={splitPos}
        onChange={size => localStorage.setItem("splitPos", size.toString())}
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
