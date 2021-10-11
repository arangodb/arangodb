import { FormProps } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import React, { ChangeEvent } from "react";
import { filter, isEmpty, negate } from "lodash";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textarea from "../../../components/pure-css/form/Textarea";

type LinkPropertiesFormProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesForm = ({ formState, dispatch, disabled, basePath }: LinkPropertiesFormProps) => {
  const updateAnalyzers = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const analyzers = event.target.value.split('\n');

    if (filter(analyzers, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: 'analyzers',
          value: analyzers
        },
        basePath
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: 'analyzers'
        },
        basePath
      });
    }
  };

  const getAnalyzers = () => {
    return (formState.analyzers || []).join('\n');
  };

  return <Grid>
    <Cell size={'1-4'}>
      <Textarea value={getAnalyzers()} onChange={updateAnalyzers} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default LinkPropertiesForm;
