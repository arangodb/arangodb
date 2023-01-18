import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { FormState, ViewContext } from "../constants";

interface LinkViewProps {
  links: FormState['links'];
  disabled: boolean | undefined;
  link: string;
  name: string;
}

const LinkView = ({
                    links,
                    disabled,
                    link
                  }: LinkViewProps) => {
  const { dispatch } = useContext(ViewContext);

  return links && links[link]
    ? <ViewLinkLayout fragments={[link]}>
      <ArangoTable style={{
        border: 'none',
        marginLeft: 0
      }}>
        <tbody>
        <tr>
          <ArangoTD seq={0} style={{
            paddingLeft: 0
          }}>
            <LinkPropertiesInput formState={links[link] || {}} disabled={disabled} dispatch={dispatch}
                                 basePath={`links[${link}]`}/>
          </ArangoTD>
        </tr>
        </tbody>
      </ArangoTable>
    </ViewLinkLayout>
    : null;
};
export default LinkView;
