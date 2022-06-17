import React, { useContext } from "react";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import { ViewContext } from "../constants";
import { Link as HashLink, Link, useRouteMatch } from "react-router-dom";

type FieldListProps = {
  fields: object | undefined;
  disabled: boolean | undefined;
  basePath: string;
};

const FieldList = ({
  fields,
  disabled,
  basePath
}: FieldListProps) => {
  const { dispatch } = useContext(ViewContext);
  const match = useRouteMatch();

  const removeField = (field: string | number) => {
    dispatch({
      type: "unsetField",
      field: {
        path: `fields[${field}]`
      },
      basePath
    });
  };

  const getFieldRemover = (field: string | number) => () => {
    removeField(field);
  };

  const fieldKeys = Object.keys(fields || {});

  return (
    <ArangoTable style={{ marginLeft: 0 }}>
      <tbody>
        {fieldKeys.length
          ? fieldKeys.map(fld =>
          <tr key={fld} style={{ borderBottom: "1px  solid #DDD" }}>
            <ArangoTD seq={0}>{fld}</ArangoTD>
            {disabled ? null : <ArangoTD seq={1} valign={"middle"}>
              <IconButton icon={"trash-o"} type={"danger"} onClick={getFieldRemover(fld)}/>
              <Link to={`${match.url}/${fld}`}><IconButton icon={"edit"} type={"warning"}/></Link>
            </ArangoTD>}
          </tr>)
          : <tr style={{ borderBottom: "1px  solid #DDD" }}>
            <ArangoTD seq={0} colSpan={2}>No sub-fields.</ArangoTD>
          </tr>
        }
      </tbody>
      <tfoot>
      <tr>
        <ArangoTD seq={0}>
          <HashLink to={`${match.url}/_add`}>
            <i className={'fa fa-plus-circle'}/>
          </HashLink>
        </ArangoTD>
      </tr>
      </tfoot>
    </ArangoTable>
  );
};

export default FieldList;
