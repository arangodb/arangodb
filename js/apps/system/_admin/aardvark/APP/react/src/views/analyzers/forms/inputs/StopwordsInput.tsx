import React, { ChangeEvent } from "react";
import { FormProps } from "../../../../utils/constants";
import { filter, isEmpty, negate } from "lodash";
import Textarea from "../../../../components/pure-css/form/Textarea";
import { StopwordsProperty } from "../../constants";

type StopwordsInputProps = FormProps<StopwordsProperty> & {
  required?: boolean
};

const StopwordsInput = ({ formState, dispatch, disabled, required = false }: StopwordsInputProps) => {
  const updateStopwords = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const stopwords = event.target.value.split('\n');

    if (filter(stopwords, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: 'properties.stopwords',
          value: stopwords
        }
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: 'properties.stopwords'
        }
      });
    }
  };

  const getStopwords = () => {
    return (formState.properties.stopwords || []).join('\n');
  };

  return <Textarea label={'Stopwords (One per line)'} value={getStopwords()} onChange={updateStopwords}
                   disabled={disabled} required={required} rows={3}/>;
};

export default StopwordsInput;
