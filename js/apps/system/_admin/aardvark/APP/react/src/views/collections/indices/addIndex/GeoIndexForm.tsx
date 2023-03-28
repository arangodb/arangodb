import React from "react";
import { IndexFormInner } from "./IndexFormInner";
import {
  FIELDS,
  INITIAL_VALUES,
  SCHEMA,
  useCreateGeoIndex
} from "./useCreateGeoIndex";

export const GeoIndexForm = ({ onClose }: { onClose: () => void }) => {
  const { onCreate } = useCreateGeoIndex();

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
