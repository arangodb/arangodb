export const encodeHelper = (url: string) => {
  const normalized = url.normalize();
  const encoded = normalized ? encodeURIComponent(normalized) : "";
  return { encoded, normalized };
};
