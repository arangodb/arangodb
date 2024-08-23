import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreatePrefixedMDIIndex
} from "./useCreatePrefixedMDIIndex";

export const MDIPrefixedIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreatePrefixedMDIIndex();

  return (
    <IndexFormInner<typeof INITIAL_VALUES, typeof FIELDS, typeof SCHEMA>
      initialValues={INITIAL_VALUES}
      schema={SCHEMA}
      fields={FIELDS}
      onCreate={onCreate}
      onClose={onClose}
    />
  );
};
