import { ArrowBackIcon } from "@chakra-ui/icons";
import { Box, Button, Spinner, Stack, Text } from "@chakra-ui/react";
import { omit } from "lodash";
import React, { useEffect, useState } from "react";
import useSWR from "swr";
import useSWRImmutable from "swr/immutable";
import { OptionType } from "../../../components/select/SelectBase";
import SingleSelect from "../../../components/select/SingleSelect";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useEditViewContext } from "../editView/EditViewContext";
import { SearchAliasViewPropertiesType, ViewDescription } from "../View.types";

export const CopyPropertiesDropdown = () => {
  const { data: views, isLoading: isLoadingList } = useSWR("/view", () =>
    getCurrentDB().listViews()
  );

  if (!views) {
    return null;
  }
  if (isLoadingList) {
    return <Spinner />;
  }
  return <CopyPropertiesInner views={views} />;
};

const CopyPropertiesInner = ({ views }: { views: ViewDescription[] }) => {
  const { onCopy, isAdminUser, initialView, setChanged } = useEditViewContext();
  const initialViewName = views?.find(
    view => view.type === initialView.type
  )?.name;
  const [options, setOptions] = useState<OptionType[]>([]);
  const [selectedViewName, setSelectedViewName] = useState(initialViewName);
  const { encoded } = encodeHelper(selectedViewName || "");
  const { data: fullViewData, isLoading } = useSWRImmutable(
    `/view/${encoded}/properties`,
    () => getCurrentDB().view(encoded).properties()
  );
  const selectedView = omit(
    fullViewData,
    "error",
    "code"
  ) as SearchAliasViewPropertiesType;
  useEffect(() => {
    if (views) {
      const newViews = views
        .filter(view => {
          return view.type === initialView.type;
        })
        .map(view => {
          return { label: view.name, value: view.name };
        });
      setOptions(newViews);
    }
  }, [views, initialView.type]);

  return (
    <Stack direction="row" alignItems="center">
      <Text>Copy mutable properties</Text>
      <Box width="44">
        <SingleSelect
          options={options}
          value={options.find(option => option.value === selectedViewName)}
          onChange={value => {
            setSelectedViewName(value?.value);
          }}
        />
      </Box>
      <Button
        size="xs"
        leftIcon={<ArrowBackIcon />}
        isLoading={isLoading}
        onClick={() => {
          onCopy({
            selectedView
          });
          if (initialView.type === "search-alias") {
            setChanged(true);
          }
        }}
        isDisabled={!isAdminUser}
      >
        Copy from
      </Button>
    </Stack>
  );
};
