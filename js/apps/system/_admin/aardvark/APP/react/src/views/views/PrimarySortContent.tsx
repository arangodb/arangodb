import React from "react";
import Textbox from "../../components/pure-css/form/Textbox";
import { FormState } from "./constants";
import { FieldNameTag } from "./FieldNameTag";

export const PrimarySortContent = ({ formState }: { formState: FormState }) => {
  if (!formState.primarySort || formState.primarySort.length === 0) {
    return (
      <table>
        <tbody>No fields set</tbody>
      </table>
    );
  }
  return (
    <table>
      <tbody>
        {formState.primarySort.map((item, index) => (
          <tr key={`${item.field})_${index}`} className="tableRow" id={"row_" + item.field}>
            <th className="collectionTh"><FieldNameTag>{item.field}</FieldNameTag>:</th>
            <th className="collectionTh">
              <Textbox
                type={"text"}
                disabled={true}
                required={false}
                value={item.asc ? "asc" : "desc"}
              />
            </th>
          </tr>
        ))}
      </tbody>
    </table>
  );
};

export const getPrimarySortTitle = ({ formState }: { formState: FormState }) => {
  const compressionInfo =
    formState.primarySortCompression !== "none"
      ? `(compression: ${formState.primarySortCompression})`
      : "";
  return `Primary Sort ${compressionInfo}`;
};
