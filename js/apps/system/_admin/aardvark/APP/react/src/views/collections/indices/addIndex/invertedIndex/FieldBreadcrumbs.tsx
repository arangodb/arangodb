import { Box, Stack, Text } from "@chakra-ui/react";
import { get, toNumber } from "lodash";
import React from "react";
import { useInvertedIndexContext } from "./InvertedIndexContext";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

/**
 * extracts last index and the rest of the path from "fields[0].nested[0].nested[0]"
 * for eg, if the path is `fields[0].nested[0].nested[0]`, then the result will be
 * `{ restPath: "fields[0].nested[0].nested" , lastIndex: 0 }`
 */

function extractLastIndexAndRestOfPath(path: string) {
  const breadcrumbPath = path.substring(0, path.lastIndexOf("["));
  const breadcrumbIndex = path.substring(
    path.lastIndexOf("[") + 1,
    path.lastIndexOf("]")
  );
  return { restPath: breadcrumbPath, lastIndex: toNumber(breadcrumbIndex) };
}

const useFieldBreakcrumbs = ({
  values,
  fullPath
}: {
  values: InvertedIndexValuesType[];
  fullPath: string | undefined;
}) => {
  const pathParts = fullPath?.split(".");
  const partsLength = pathParts?.length; // length 1 means not nested at all
  const breadcrumbs =
    pathParts &&
    new Array(partsLength).fill(null).map((_, index) => {
      const breadcrumbPath = pathParts.slice(0, index + 1).join(".");
      const breadcrumbValue = get(values, `${breadcrumbPath}.name`)!;
      const { restPath, lastIndex } = extractLastIndexAndRestOfPath(
        breadcrumbPath
      );
      return {
        breadcrumbPath: restPath,
        breadcrumbValue,
        breakcrumbIndex: lastIndex
      };
    });
  return breadcrumbs;
};
export const FieldBreadcrumbs = ({
  values,
  fullPath
}: {
  values: InvertedIndexValuesType[];
  fullPath: string;
}) => {
  const breadcrumbs = useFieldBreakcrumbs({
    values,
    fullPath
  });
  const { setCurrentFieldData, currentFieldData } = useInvertedIndexContext();

  return (
    <Stack
      direction="row"
      fontSize="md"
      background={"gray.800"}
      color="white"
      paddingX="4"
      paddingY="2"
    >
      <Text
        cursor="pointer"
        textDecoration="underline"
        onClick={() => {
          setCurrentFieldData(undefined);
        }}
      >
        Fields
      </Text>
      <Text>{">>"}</Text>
      {breadcrumbs?.map((breadcrumb, index) => {
        const isCurrentBreadcrumb =
          currentFieldData?.fieldIndex === breadcrumb.breakcrumbIndex &&
          currentFieldData?.fieldValue === breadcrumb.breadcrumbValue;
        return (
          <Stack direction="row" key={breadcrumb.breadcrumbPath}>
            {index > 0 ? <Box>{">>"}</Box> : null}
            <Box
              textDecoration={!isCurrentBreadcrumb ? "underline" : ""}
              cursor={!isCurrentBreadcrumb ? "pointer" : ""}
              onClick={() => {
                !isCurrentBreadcrumb &&
                  setCurrentFieldData({
                    fieldIndex: breadcrumb.breakcrumbIndex,
                    fieldValue: breadcrumb.breadcrumbValue,
                    fieldName: breadcrumb.breadcrumbPath
                  });
              }}
            >
              {breadcrumb.breadcrumbValue}
            </Box>
          </Stack>
        );
      })}
    </Stack>
  );
};
