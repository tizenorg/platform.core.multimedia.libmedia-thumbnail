. ./_export_env.sh                              # setting environment variables

echo PATH=$PATH
echo LD_LIBRARY_PATH=$TET_TARGET_PATH/lib/tet3:$LD_LIBRARY_PATH
echo TET_ROOT=$TET_ROOT
echo TET_SUITE_ROOT=$TET_SUITE_ROOT
echo ARCH=$ARCH

RESULT_DIR=results-$ARCH
HTML_RESULT=$RESULT_DIR/build-tar-result-$FILE_NAME_EXTENSION.html
JOURNAL_RESULT=$RESULT_DIR/build-tar-result-$FILE_NAME_EXTENSION.journal

mkdir $RESULT_DIR

tcc -c -p ./                                # executing tcc, with clean option (-c)
tcc -b -j $JOURNAL_RESULT -p ./            # executing tcc to build test cases (-b)
grw -c 3 -f chtml -o $HTML_RESULT $JOURNAL_RESULT  # reporting the result

