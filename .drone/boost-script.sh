#!/bin/bash

set -ex

export TRAVIS_BUILD_DIR=$(pwd)
export TRAVIS_BRANCH=$DRONE_BRANCH

echo '==================================> BEFORE_INSTALL'

if  [ "$DRONE_BEFORE_INSTALL" = "beast_coverage" ]; then
    pip install --user https://github.com/codecov/codecov-python/archive/master.zip
    wget http://downloads.sourceforge.net/ltp/lcov-1.14.tar.gz
    tar -xvf lcov-1.14.tar.gz
    cd lcov-1.14
    make install
    cd ..
fi

if  [ "$DRONE_BEFORE_INSTALL" = "UBasan" ]; then
    export PATH=$PWD/llvm-$LLVM_VERSION/bin:$PATH
fi

echo '==================================> INSTALL'

cd ..
$TRAVIS_BUILD_DIR/tools/get-boost.sh $TRAVIS_BRANCH $TRAVIS_BUILD_DIR
cd boost-root
export PATH=$PATH:"`pwd`"
export BOOST_ROOT=$(pwd)
./bootstrap.sh
cp libs/beast/tools/user-config.jam ~/user-config.jam
echo "using $TOOLSET : : $COMPILER : $CXX_FLAGS ;" >> ~/user-config.jam

echo '==================================> COMPILE'

cd ../boost-root
libs/beast/tools/retry.sh libs/beast/tools/build-and-test.sh

