import Ajv from "ajv";
import ajvErrors from 'ajv-errors';
import {
  AnalyzerTypeState,
  AqlState,
  CollationState,
  DelimiterState,
  formSchema,
  GeoJsonState,
  GeoPointState,
  NGramState,
  NormState,
  PipelineStates,
  SegmentationState,
  StemState,
  StopwordsState,
  TextState
} from "./constants";
import { DispatchArgs, FormProps } from "../../utils/constants";
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

export function getForm (formProps: FormProps<AnalyzerTypeState>) {
  const typeName = formProps.formState.type;

  switch (typeName) {
    case 'identity':
      return null;

    case 'delimiter': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<DelimiterState>(formProps);

      return <DelimiterForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'stem': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<StemState>(formProps);

      return <StemForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'norm': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<NormState>(formProps);

      return <NormForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'ngram': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<NGramState>(formProps);

      return <NGramForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'text': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<TextState>(formProps);

      return <TextForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'aql': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<AqlState>(formProps);

      return <AqlForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'stopwords': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<StopwordsState>(formProps);

      return <StopwordsForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'collation': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<CollationState>(formProps);

      return <CollationForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'segmentation': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<SegmentationState>(formProps);

      return <SegmentationForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'pipeline': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<PipelineStates>(formProps);

      return <PipelineForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'geojson': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<GeoJsonState>(formProps);

      return <GeoJsonForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }

    case 'geopoint': {
      const { disabled, dispatch, formState } = getConstrainedFormProps<GeoPointState>(formProps);

      return <GeoPointForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
    }
  }
}

export function getConstrainedFormProps<T extends AnalyzerTypeState> (formProps: FormProps<AnalyzerTypeState>): FormProps<T> {
  return {
    formState: formProps.formState as T,
    dispatch: formProps.dispatch as Dispatch<DispatchArgs<T>>,
    disabled: formProps.disabled
  };
}
