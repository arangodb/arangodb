file(READ "${SRC}" HEX_DATA HEX)
string(REGEX REPLACE "(..)" "0x\\1, " CS_HEX_DATA "${HEX_DATA}")
file(WRITE "${DST}" "${CS_HEX_DATA}")
