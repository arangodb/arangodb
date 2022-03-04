import React from "react";
import ViewLayout from "./ViewLayout";
import { ArangoTD } from "../../../components/arango/table";
import AutoCompleteTextInput from "../../../components/pure-css/form/AutoCompleteTextInput";
import { IconButton } from "../../../components/arango/buttons";

type NewLinkProps = {
  disabled: boolean;
  options: string[] | number[];
  collection: string | number;
  addLink: Function;
  updateCollection: (value: string | number) => void;
  view: string | undefined;
};

const NewLink: React.FC<NewLinkProps> = ({
  collection,
  options,
  updateCollection,
  disabled,
  addLink,
  view
}) => {
  return (
    <ViewLayout view={view} disabled={disabled}>
      <tr style={{ borderBottom: "1px  solid #DDD" }}>
        <ArangoTD seq={0} colSpan={2}>
          <AutoCompleteTextInput
            placeholder={"Collection"}
            value={collection}
            minChars={1}
            spacer={""}
            onSelect={updateCollection}
            matchAny={true}
            options={options}
            onChange={updateCollection}
          />
        </ArangoTD>
        <ArangoTD seq={1} style={{ width: "20%" }}>
          <IconButton
            icon={"plus"}
            type={"warning"}
            onClick={addLink}
            disabled={disabled}
          >
            Add
          </IconButton>
        </ArangoTD>
      </tr>
    </ViewLayout>
  );
};

export default NewLink;
