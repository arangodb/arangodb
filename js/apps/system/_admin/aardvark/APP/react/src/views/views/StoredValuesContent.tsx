import React from "react";
import Textbox from "../../components/pure-css/form/Textbox";
import { FormState } from "./constants";
import { FieldNameTag } from "./FieldNameTag";

export const StoredValuesContent = ({
  formState
}: {
  formState: FormState;
}) => {
  if (!formState.storedValues || formState.storedValues.length === 0) {
    return (
      <table>
        <tbody>No fields set</tbody>
      </table>
    );
  }
  return (
    <table>
      <tbody>
        {formState.storedValues.map(item => (
          <tr className="tableRow" id={"row_" + item.fields}>
            <th className="collectionTh">
              {item.fields.map(field => {
                return <FieldNameTag>{field}</FieldNameTag>;
              })}
              :
            </th>
            <th className="collectionTh">
              <Textbox
                type={"text"}
                disabled={true}
                required={false}
                value={item.compression}
              />
            </th>
            <th className="collectionTh"></th>
          </tr>
        ))}
      </tbody>
    </table>
  );
};
