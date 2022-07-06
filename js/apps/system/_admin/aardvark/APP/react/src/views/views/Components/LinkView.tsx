import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { FormState, ViewContext } from "../constants";
import { SaveButton } from "../Actions";

interface LinkViewProps {
  links: FormState['links'];
  disabled: boolean | undefined;
  link: string;
  name: string;
}

const LinkView = ({
                    links,
                    disabled,
                    link,
                    name
                  }: LinkViewProps) => {
  const { formState, dispatch, isAdminUser, changed, setChanged } = useContext(ViewContext);

  return links && links[link]
    ? <ViewLinkLayout fragments={[link]}>
      <ArangoTable style={{ border: 'none' }}>
        <tbody>
        <tr>
          <ArangoTD seq={0}>
            <LinkPropertiesInput formState={links[link] || {}} disabled={disabled} dispatch={dispatch}
                                 basePath={`links[${link}]`}/>
          </ArangoTD>
        </tr>
        </tbody>
      </ArangoTable>
      {
        isAdminUser && changed
          ? <div className="tab-pane tab-pane-modal active" id="Save">
            <SaveButton view={formState as FormState} setChanged={setChanged} oldName={name}/>
          </div>
          : null
      }
    </ViewLinkLayout>
    : null;
};
export default LinkView;
