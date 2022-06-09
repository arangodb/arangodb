import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { FormState, ViewContext } from "../constants";

interface LinkViewProps {
  links: FormState['links'];
  disabled: boolean | undefined;
  link: string;
}

const LinkView = ({
                    links,
                    disabled,
                    link
                  }: LinkViewProps) => {
  const { dispatch } = useContext(ViewContext);

  return links && links[link]
    ? <ViewLinkLayout fragments={[link]}>
      <ArangoTable>
        <tbody>
        <tr style={{ borderBottom: "1px  solid #DDD" }}>
          <ArangoTD seq={0}>
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
