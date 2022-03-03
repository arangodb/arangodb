import React, { Dispatch } from "react";
import ViewLayout from "./ViewLayout";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { ArangoTD } from "../../../components/arango/table";
import { map } from "lodash";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";

interface LinkViewProps {
  links: {}[];
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  view?: string | undefined;
  link?: string;
}

const LinkView: React.FC<LinkViewProps> = ({
  links,
  disabled,
  dispatch,
  view,
  link
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

  return (
    <ViewLayout view={view} disabled={disabled} link={link}>
      {map(links, (properties, coll) => (
        <tr key={coll} style={{ borderBottom: "1px  solid #DDD" }}>
          <ArangoTD seq={disabled ? 1 : 2}>
            <LinkPropertiesInput
              formState={properties}
              disabled={disabled || !properties}
              dispatch={dispatch}
              basePath={`links[${coll}]`}
            />
          </ArangoTD>

          {/* <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD> */}
          {disabled ? null : (
            <ArangoTD seq={0} valign={"middle"}>
              <IconButton
                icon={"trash-o"}
                type={"danger"}
                onClick={getLinkRemover(coll)}
              />
            </ArangoTD>
          )}
        </tr>
      ))}
    </ViewLayout>
  );
};
export default LinkView;
