import { FormProps } from "../../../utils/constants";
import { FormState } from "../constants";
import React from "react";
import { ArangoTable } from "../../../components/arango/table";

const LinkPropertiesForm = ({ formState, dispatch, disabled }: FormProps<FormState>) => {
  const links = formState.links || {};

  const removeLink = (collection: string) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links${collection}`,
        value: null
      }
    });
  };

  const getRemover = (collection: string) => () => {
    removeLink(collection);
  };

  const addLink = (collection: string) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
  };

  return <ArangoTable>

  </ArangoTable>;
};

export default LinkPropertiesForm;
