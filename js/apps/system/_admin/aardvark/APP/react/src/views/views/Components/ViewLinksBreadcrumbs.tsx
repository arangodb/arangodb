import { Box, HStack, Text } from "@chakra-ui/react";
import React from "react";
import { useLinksContext } from "../LinksContext";

/**
 * input: links[a].fields[b].fields[c]
 * output: [['links', 'a'], ['fields', 'b], ['fields', 'c']]
 */
const getFragments = (currentPath?: string) => {
  if (!currentPath) {
    return [];
  }
  // split by links[ and fields[
  // const pathParts = currentPath.split(".");
  // the above line doesn't work because field can contain dot
  // so we need to split by .fields[ and .links[
  const linksFieldPath = currentPath
    // split on links
    .split("links[")[1]
    // split on fields to extract just the links[] part
    .split(".fields[")[0];
  const linkPathValue = linksFieldPath.slice(
    0,
    linksFieldPath.lastIndexOf("]")
  );
  const fieldPathParts = currentPath
    .split(".fields[")
    // remove the first item because it'll be the links[] part
    .slice(1);
  // we need to append both the parts
  const linkPathParts = ["links", linkPathValue];
  let fragments = [linkPathParts]; // should be [['links', 'a'], ['fields', 'b], ['fields', 'c']]
  fieldPathParts.forEach(pathPart => {
    fragments = [
      ...fragments,
      ["fields", pathPart.slice(0, pathPart.lastIndexOf("]"))]
    ];
  });
  return fragments;
};

/**
 * this function creates a path from the fragments
 * input: [['links', 'a'], ['fields', 'b], ['fields', 'c']]
 * output: links[a].fields[b].fields[c]

 *  */
const getPathFromFragments = (fragments: string[][]) => {
  let path = "";
  fragments.forEach((fragment, index) => {
    if (index === 0) {
      path = fragment[0] + "[" + fragment[1] + "]";
    } else {
      path = path + "." + fragment[0] + "[" + fragment[1] + "]";
    }
  });
  return path;
};

export const ViewLinksBreadcrumbs = () => {
  const { setCurrentField, currentField } = useLinksContext();
  const fragments = getFragments(currentField?.fieldPath);
  return (
    <Box fontSize="md">
      <Box backgroundColor="gray.800" paddingX="4" paddingY="2">
        <HStack listStyleType="none" as="ul" color="gray.100" flexWrap="wrap">
          <Box
            textDecoration="underline"
            _hover={{ color: "gray.100" }}
            cursor="pointer"
            onClick={() => setCurrentField(undefined)}
          >
            Links
          </Box>
          <Box>{">>"}</Box>
          {fragments
            .slice(0, fragments.length - 1)
            .map(([_path, fragment], idx) => {
              return (
                <HStack as="li" key={`${idx}-${fragment}`}>
                  <Box
                    textDecoration="underline"
                    _hover={{ color: "gray.100" }}
                    cursor="pointer"
                    isTruncated
                    maxWidth="200px"
                    title={fragment}
                    onClick={() => {
                      // this gets the fragments array until the current breadcrumb's index
                      // used to create the path
                      const currentFragments = fragments.slice(0, idx + 1);
                      const path = getPathFromFragments(currentFragments);
                      path &&
                        setCurrentField({
                          fieldPath: path
                        });
                    }}
                  >
                    {fragment}
                  </Box>
                  <Box>{">>"}</Box>
                </HStack>
              );
            })}
          <Text
            isTruncated
            maxWidth="200px"
            title={fragments[fragments.length - 1]?.[1]}
          >
            {fragments[fragments.length - 1]?.[1]}
          </Text>
        </HStack>
      </Box>
    </Box>
  );
};
