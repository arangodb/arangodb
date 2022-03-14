import React, { Dispatch } from "react";
import ViewLayout from "./ViewLayout";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { ArangoTD } from "../../../components/arango/table";
// import { map } from "lodash";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";

interface LinkViewProps {
  links: {};
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  view?: string | undefined;
  link?: string;
  field?: string;
}

const LinkView: React.FC<LinkViewProps> = ({
  links,
  disabled,
  dispatch,
  view,
  link,
  field
}) => {
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
        const link = links[l];
        return link;
      }
    }
  };
  const newLink = loopLinks(links);

  return (
    <ViewLayout view={view} disabled={disabled} link={link} field={field}>
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
    </ViewLayout>
  );
};
export default LinkView;
