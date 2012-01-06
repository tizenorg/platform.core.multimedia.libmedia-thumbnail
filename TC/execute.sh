#!/bin/sh

export ARCH=target

export TET_INSTALL_PATH=/mnt/nfs/tetware/TETware  # path to mount
export TET_TARGET_PATH=$TET_INSTALL_PATH/tetware-target
export PATH=$TET_TARGET_PATH/bin:$PATH
export LD_LIBRARY_PATH=$TET_TARGET_PATH/lib/tet3:$LD_LIBRARY_PATH

export TET_ROOT=$TET_TARGET_PATH

set $(pwd)
export TET_SUITE_ROOT=$1

set $(date +%s)
FILE_NAME_EXTENSION=$1

echo PATH=$PATH
echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
echo TET_ROOT=$TET_ROOT
echo TET_SUITE_ROOT=$TET_SUITE_ROOT
echo ARCH=$ARCH

RESULT_DIR=results-$ARCH
HTML_RESULT=$RESULT_DIR/build-tar-result-$FILE_NAME_EXTENSION.html
JOURNAL_RESULT=$RESULT_DIR/build-tar-result-$FILE_NAME_EXTENSION.journal

mkdir $RESULT_DIR

tcc -e -j $JOURNAL_RESULT -p ./
grw -c 3 -f chtml -o $HTML_RESULT $JOURNAL_RESULT  # reporting the result

