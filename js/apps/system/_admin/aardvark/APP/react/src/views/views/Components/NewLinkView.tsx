import React, { Dispatch } from "react";
import ViewLayout from "./ViewLayout";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { ArangoTD } from "../../../components/arango/table";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";

type NewLinkViewProps = {
  link: {} | any;
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  view?: string | undefined;
  linkName?: string;
};

const NewLinkView: React.FC<NewLinkViewProps> = ({
  link,
  disabled,
  dispatch,
  view,
  linkName
}) => {
  const removeLink = (collection: {}) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  const getLinkRemover = (collection: {}) => () => {
    removeLink(collection);
  };

  return (
    <ViewLayout view={view} disabled={disabled} link={linkName}>
      <tr style={{ borderBottom: "1px  solid #DDD" }}>
        <ArangoTD seq={disabled ? 1 : 2}>
          <LinkPropertiesInput
            formState={link.properties}
            disabled={disabled || !link.properties}
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
              onClick={getLinkRemover(link)}
            />
          </ArangoTD>
        )}
      </tr>
    </ViewLayout>
  );
};

export default NewLinkView;
