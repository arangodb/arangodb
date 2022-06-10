import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import AutoCompleteTextInput from "../../../components/pure-css/form/AutoCompleteTextInput";
import { IconButton } from "../../../components/arango/buttons";
import { ViewContext } from "../constants";
import { useHistory } from "react-router-dom";
import { NavButton } from "../Actions";

type NewLinkProps = {
  disabled: boolean;
  options: string[] | number[];
  collection: string | number;
  updateCollection: (value: string | number) => void;
};

const NewLink = ({
                   collection,
                   options,
                   updateCollection,
                   disabled
                 }: NewLinkProps) => {
  const { dispatch } = useContext(ViewContext);
  const history = useHistory();

  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    history.goBack();
  };

  return (
    <ViewLinkLayout fragments={['[New]']}>
      <ArangoTable>
        <tbody>
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
        </tbody>
        <tfoot>
        <tr><ArangoTD seq={0}><NavButton arrow={'left'} text={'Back'}/></ArangoTD></tr>
        </tfoot>
      </ArangoTable>
    </ViewLinkLayout>
  );
};

export default NewLink;
