import Ajv from "ajv";
import ajvErrors from 'ajv-errors';
import { AnalyzerTypeState, DispatchArgs, FormProps, formSchema } from "./constants";
import DelimiterForm from "./forms/DelimiterForm";
import StemForm from "./forms/StemForm";
import NormForm from "./forms/NormForm";
import NGramForm from "./forms/NGramForm";
import TextForm from "./forms/TextForm";
import AqlForm from "./forms/AqlForm";
import GeoJsonForm from "./forms/GeoJsonForm";
import GeoPointForm from "./forms/GeoPointForm";
import React, { Dispatch } from "react";
import PipelineForm from "./forms/PipelineForm";
import { parseInt } from "lodash";
import StopwordsForm from "./forms/StopwordsForm";
import CollationForm from "./forms/CollationForm";
import SegmentationForm from "./forms/SegmentationForm";

const ajv = new Ajv({
  allErrors: true,
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);

export const validateAndFix = ajv.compile(formSchema);

export const getForm = ({ formState, dispatch, disabled = true }: FormProps) => {
  const typeName = (formState as AnalyzerTypeState).type;

  switch (typeName) {
    case 'identity':
      return null;

    case 'delimiter':
      return <DelimiterForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'stem':
      return <StemForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'norm':
      return <NormForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'ngram':
      return <NGramForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'text':
      return <TextForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'aql':
      return <AqlForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'stopwords':
      return <StopwordsForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'collation':
      return <CollationForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'segmentation':
      return <SegmentationForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'pipeline':
      return <PipelineForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'geojson':
      return <GeoJsonForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'geopoint':
      return <GeoPointForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
  }
};

export const setIntegerField = (field: string, value: string, dispatch: Dispatch<DispatchArgs>, basePath?: string) => {
  const numValue = parseInt(value);

  if (numValue) {
    dispatch({
      type: 'setField',
      field: {
        path: field,
        value: numValue
      },
      basePath
    });
  } else {
    dispatch({
      type: 'unsetField',
      field: {
        path: field
      },
      basePath
    });
  }
};
