export const createEncodedUrl = ({
  path,
  value
}: {
  path: string;
  value: string;
}) => {
  return `${window.location.origin}${
    window.location.pathname
  }#${path}/${encodeURIComponent(value)}`;
};
