export const encodeHelper = (url: string) => {
  const normalized = url.normalize();
  const encoded = normalized ? encodeURIComponent(normalized) : "";
  const advanced = normalized
    ? encodeURIComponent(normalized.replace(/%/g, "%25"))
    : "";
  return { encoded, normalized, advanced };
};
