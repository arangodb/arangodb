import { FormState } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React, { useEffect, useState } from "react";
import { find, sortBy, pick } from "lodash";
import useSWRImmutable from "swr/immutable";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { validateAndFix } from "../../helpers";
import { useHistory, useLocation } from "react-router-dom";
import { Box, Button, Stack, Text } from "@chakra-ui/react";
import { ArrowBackIcon } from "@chakra-ui/icons";
import SingleSelect from "../../../../components/select/SingleSelect";

type CopyFromInputProps = {
  views: FormState[];
} & Pick<FormProps<FormState>, 'dispatch' | 'formState'>;


const filterAndSortViews = (views: FormState[]) => {
  return sortBy(
    views
      .filter(view => {
        return view.type === "arangosearch";
      })
      .map(view => {
        return { value: view.name, label: view.name };
      }),
    "name"
  );
};

const CopyFromInput = ({ views, dispatch, formState }: CopyFromInputProps) => {
  const initalViewOptions = filterAndSortViews(views);
  const [viewOptions, setViewOptions] = useState(initalViewOptions);
  const [selectedView, setSelectedView] = useState(viewOptions[0]);
  const { data } = useSWRImmutable(`/view/${selectedView.value}/properties`, (path) => getApiRouteForCurrentDB().get(path));
  const location = useLocation();
  const history = useHistory();
  const fullView = data ? data.body : selectedView;

  useEffect(() => {
    setViewOptions(filterAndSortViews(views));
  }, [views]);

  const copyFormState = () => {
    validateAndFix(fullView);
    Object.assign(fullView, pick(formState, 'id', 'name', 'primarySort', 'primarySortCompression',
      'storedValues', 'writebufferIdle', 'writebufferActive', 'writebufferSizeMax'));

    dispatch({
      type: 'setFormState',
      formState: fullView as FormState
    });
    dispatch({ type: 'regenRenderKey' });
    if(location) {
      history.push("/"); 
    }
  };

  const updateSelectedView = (value: string) => {
    const tempSelectedView = find(viewOptions, { value }) || viewOptions[0];

    setSelectedView(tempSelectedView);
  };


  return (
    <Stack direction="row" alignItems="center" flexWrap="wrap">
      <Text>Copy mutable properties</Text>
      <Stack direction="row" alignItems="center">
        <Box width="44">
          <SingleSelect
            options={viewOptions}
            value={viewOptions.find(
              option => option.value === selectedView.value
            )}
            onChange={value => {
              updateSelectedView((value as any).value);
            }}
          />
        </Box>
        <Button
          size="xs"
          leftIcon={<ArrowBackIcon />}
          onClick={copyFormState}
        >
          Copy from
        </Button>
      </Stack>
    </Stack>
  );
};

export default CopyFromInput;
