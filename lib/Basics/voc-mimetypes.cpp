////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from mimetypes.dat
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Common.h>
#include "./lib/Basics/voc-mimetypes.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseEntriesMimetypes (void) {
  TRI_RegisterMimetype("gif", "image/gif", false);
  TRI_RegisterMimetype("jpg", "image/jpg", false);
  TRI_RegisterMimetype("png", "image/png", false);
  TRI_RegisterMimetype("ico", "image/x-icon", false);
  TRI_RegisterMimetype("css", "text/css", true);
  TRI_RegisterMimetype("js", "text/javascript", true);
  TRI_RegisterMimetype("json", "application/json", true);
  TRI_RegisterMimetype("html", "text/html", true);
  TRI_RegisterMimetype("htm", "text/html", true);
  TRI_RegisterMimetype("pdf", "application/pdf", false);
  TRI_RegisterMimetype("txt", "text/plain", true);
  TRI_RegisterMimetype("text", "text/plain", true);
  TRI_RegisterMimetype("xml", "application/xml", true);
  TRI_RegisterMimetype("svg", "image/svg+xml", true);
  TRI_RegisterMimetype("ttf", "application/x-font-ttf", false);
  TRI_RegisterMimetype("otf", "application/x-font-opentype", false);
  TRI_RegisterMimetype("woff", "application/font-woff", false);
  TRI_RegisterMimetype("eot", "application/vnd.ms-fontobject", false);
}
