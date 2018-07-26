#!/bin/bash
export TEST_BIN_NAME=$1
#echo "Building ${TEST_BIN_NAME}"
shift 1
export BUILD_COMMAND=$@
#echo "Build command: ${BUILD_COMMAND}"
eval ${BUILD_COMMAND} >/dev/null 2>/dev/null

if [ $? -eq 0 ]; then
	echo -ne "#!/bin/bash\nexit 1;" > ${TEST_BIN_NAME}
else
	echo -ne "#!/bin/bash\nexit 0;" > ${TEST_BIN_NAME}
fi
chmod u+x ${TEST_BIN_NAME}
exit 0;

