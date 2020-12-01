#!/bin/bash

set -ex

export BOOST_CI_SRC_FOLDER=$(pwd)

echo '==================================> INSTALL'
pwd
cd ..
mkdir -p $HOME/cache && cd $HOME/cache
if [ ! -d doxygen ]; then git clone -b 'Release_1_8_15' --depth 1 https://github.com/doxygen/doxygen.git && echo "not-cached" ; else echo "cached" ; fi
cd doxygen
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cd build
sudo make install
cd ../..
if [ ! -f saxonhe.zip ]; then wget -O saxonhe.zip https://sourceforge.net/projects/saxon/files/Saxon-HE/9.9/SaxonHE9-9-1-4J.zip/download && echo "not-cached" ; else echo "cached" ; fi
unzip -o saxonhe.zip
sudo rm /usr/share/java/Saxon-HE.jar
sudo cp saxon9he.jar /usr/share/java/Saxon-HE.jar
cd ..
BOOST_BRANCH=develop && [ "$DRONE_BRANCH" == "master" ] && BOOST_BRANCH=master || true
git clone -b $BOOST_BRANCH https://github.com/boostorg/boost.git boost-root --depth 1
cd boost-root
export BOOST_ROOT=$(pwd)
git submodule update --init libs/context
git submodule update --init tools/boostbook
git submodule update --init tools/boostdep
git submodule update --init tools/docca
git submodule update --init tools/quickbook
rsync -av $BOOST_CI_SRC_FOLDER/ libs/json
python tools/boostdep/depinst/depinst.py ../tools/quickbook
./bootstrap.sh
./b2 headers

echo '==================================> COMPILE'

echo "using doxygen ; using boostbook ; using saxonhe ;" > tools/build/src/user-config.jam
./b2 -j3 libs/json/doc//boostrelease
