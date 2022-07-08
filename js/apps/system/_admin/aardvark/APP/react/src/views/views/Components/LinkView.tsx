import React, { useContext } from "react";
import ViewLayout from "./ViewLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import { ViewContext } from "../ViewLinksReactView";

interface LinkViewProps {
  links: {};
  disabled: boolean | undefined;
  view?: string | undefined;
  link?: string;
  field?: string;
}

const LinkView = ({
  links,
  disabled,
  view,
  link,
  field
}: LinkViewProps) => {
  const { dispatch } = useContext(ViewContext);

  const removeLink = (collection: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  const getLinkRemover = (collection: string | number) => () => {
    removeLink(collection);
  };

  const loopLinks = (links: object | any) => {
    for (const l in links) {
      if (link === l) {
        return links[l];
      }
    }
  };
  const newLink = loopLinks(links);

  return (
    <ViewLayout view={view} link={link} field={field}>
      <ArangoTable>
        <tbody>
          <tr key={newLink} style={{ borderBottom: "1px  solid #DDD" }}>
          <ArangoTD seq={disabled ? 1 : 2}>
            <LinkPropertiesInput
              formState={newLink}
              disabled={disabled || !newLink}
              dispatch={dispatch}
              basePath={`links[${link}]`}
            />
          </ArangoTD>

          {/* <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD> */}
          {disabled ? null : (
            <ArangoTD seq={0} valign={"middle"}>
              <IconButton
                icon={"trash-o"}
                type={"danger"}
                onClick={getLinkRemover(newLink)}
              />
            </ArangoTD>
          )}
        </tr>
        </tbody>
      </ArangoTable>
    </ViewLayout>
  );
};
export default LinkView;
