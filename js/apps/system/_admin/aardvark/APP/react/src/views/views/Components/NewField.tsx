import React, { ChangeEvent, useContext } from "react";
import { ViewContext } from "../constants";
import { useHistory, useLocation, useRouteMatch } from "react-router-dom";
import { useLinkState } from "../helpers";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import { NavButton } from "../Actions";
import Textbox from "../../../components/pure-css/form/Textbox";

const NewField = () => {
  const { dispatch, formState } = useContext(ViewContext);
  const history = useHistory();
  const location = useLocation();
  const match = useRouteMatch();

  const fragments = match.url.slice(1).split('/').slice(0, -1);
  let basePath = `links[${fragments[0]}]`;
  for (let i = 1; i < fragments.length; ++i) {
    basePath += `.fields[${fragments[i]}]`;
  }

  const [newField, setNewField, addDisabled] = useLinkState(formState, `${basePath}.fields`);

  const up = location.pathname.slice(0, location.pathname.lastIndexOf('/'));

  const addField = () => {
    dispatch({
      type: "setField",
      field: {
        path: `fields[${newField}]`,
        value: {}
      },
      basePath
    });
    history.push(up);
  };

  const updateNewField = (event: ChangeEvent<HTMLInputElement>) => {
    setNewField(event.target.value);
  };

  return <ViewLinkLayout fragments={fragments.concat('[New Field]')}>
    <ArangoTable>
      <tbody>
      <tr style={{ borderBottom: "1px  solid #DDD" }}>
        <ArangoTD seq={0} style={{ width: "80%" }}>
          <Textbox placeholder={'Field'} type={'text'} value={newField} onChange={updateNewField}/>
          { newField && addDisabled ? <div style={{ color: 'red' }}>This field already exists</div> : null }
        </ArangoTD>
        <ArangoTD seq={1} style={{ width: "20%" }}>
          <IconButton icon={"plus"} type={"warning"} onClick={addField} disabled={addDisabled}>
            Add
          </IconButton>
        </ArangoTD>
      </tr>
      </tbody>
      <tfoot>
      <tr><ArangoTD seq={0}><NavButton arrow={'left'} text={'Back'}/></ArangoTD></tr>
      </tfoot>
    </ArangoTable>
  </ViewLinkLayout>;
};

export default NewField;
