import React from "react";
import { Route, Switch } from "react-router-dom";
import SplitPane from "react-split-pane";
import { State } from "../../utils/constants";
import AccordionView from "./Components/Accordion/Accordion";
import LinkList from "./Components/LinkList";
import { FormState } from "./constants";
import ConsolidationPolicyForm from "./forms/ConsolidationPolicyForm";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import { GeneralContent } from "./GeneralContent";
import { getPrimarySortTitle, PrimarySortContent } from "./PrimarySortContent";
import { StoredValuesContent } from "./StoredValuesContent";
import useElementSize from "./useElementSize";
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
  dispatch: () => void;
  isAdminUser: boolean;
  state: State<FormState>;
}) => {
  const [sectionRef, sectionSize] = useElementSize();
  const sectionWidth = sectionSize.width;
  const maxSize = sectionWidth - 200;
  const localStorageSplitPos = localStorage.getItem("splitPos") || "0";
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
        <div style={{ marginRight: "15px" }}>
          <AccordionView
            allowMultipleOpen
            accordionConfig={[
              {
                index: 0,
                content: (
                  <div>
                    <Switch>
                      <Route path={"/:link"}>
                        <LinkList />
                        <LinkPropertiesForm name={name} />
                      </Route>
                      <Route exact path={"/"}>
                        <LinkList />
                      </Route>
                    </Switch>
                  </div>
                ),
                label: "Links",
                testID: "accordionItem0",
                defaultActive: true
              },
              {
                index: 1,
                content: (
                  <div>
                    <GeneralContent
                      formState={formState}
                      dispatch={dispatch}
                      isAdminUser={isAdminUser}
                    />
                  </div>
                ),
                label: "General",
                testID: "accordionItem1"
              },
              {
                index: 2,
                content: (
                  <div>
                    <ConsolidationPolicyForm
                      formState={formState}
                      dispatch={dispatch}
                      disabled={!isAdminUser}
                    />
                  </div>
                ),
                label: "Consolidation Policy",
                testID: "accordionItem2"
              },
              {
                index: 3,
                content: (
                  <div>
                    <PrimarySortContent formState={formState} />
                  </div>
                ),
                label: getPrimarySortTitle({formState}),
                testID: "accordionItem3"
              },
              {
                index: 4,
                content: (
                  <div>
                    <StoredValuesContent formState={formState} />
                  </div>
                ),
                label: "Stored Values",
                testID: "accordionItem4"
              }
            ]}
          />
        </div>
        <ViewRightPane
          formState={formState}
          dispatch={dispatch}
          state={state}
        />
      </SplitPane>
    </section>
  );
};
