import { FormState } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React, { ChangeEvent, useEffect, useState } from "react";
import { find, sortBy, pick } from "lodash";
import useSWRImmutable from "swr/immutable";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { validateAndFix } from "../../helpers";
import { IconButton } from "../../../../components/arango/buttons";
import { useHistory, useLocation } from "react-router-dom";
import { Stack, Text } from "@chakra-ui/react";

type CopyFromInputProps = {
  views: FormState[];
} & Pick<FormProps<FormState>, 'dispatch' | 'formState'>;

const CopyFromInput = ({ views, dispatch, formState }: CopyFromInputProps) => {
  const [sortedViews, setSortedViews] = useState(sortBy(views, 'name'));
  const [selectedView, setSelectedView] = useState(sortedViews[0]);
  const { data } = useSWRImmutable(`/view/${selectedView.name}/properties`, (path) => getApiRouteForCurrentDB().get(path));
  const location = useLocation();
  const history = useHistory();
  const fullView = data ? data.body : selectedView;

  useEffect(() => {
    setSortedViews(sortBy(views, 'name'));
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

  const updateSelectedView = (event: ChangeEvent<HTMLSelectElement>) => {
    const tempSelectedView = find(sortedViews, { name: event.target.value }) || sortedViews[0];

    setSelectedView(tempSelectedView);
  };


  return (
    <Stack direction="row" alignItems="center" flexWrap="wrap">
      <Text>Copy mutable properties</Text>
      <Stack direction="row" alignItems="center">
        <select
          value={selectedView.name}
          onChange={updateSelectedView}
          style={{ marginBottom: "0px" }}
        >
          {sortedViews.map((view, idx) => (
            <option key={idx} value={view.name}>
              {view.name}
            </option>
          ))}
        </select>
        <IconButton icon={"hand-o-left"} type={"warning"} onClick={copyFormState}>
          Copy
        </IconButton>
      </Stack>
    </Stack>
  );
};

export default CopyFromInput;
