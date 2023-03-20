#!/bin/bash

# Copyright 2020 Rene Rivera, Sam Darwin
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://boost.org/LICENSE_1_0.txt)

set -xe

echo "==================================> ENVIRONMENT"

export BOOST_CI_SRC_FOLDER=$(pwd)
export PATH=~/.local/bin:/usr/local/bin:$PATH
printenv

echo "==================================> PACKAGES"

for package in ${PACKAGES// / }; do
  if [[ "$package" == "--"* ]]; then
    continue
  fi
  package_no_version=${package%%=*}
  echo "Versions available for $package_no_version"
  apt-cache policy "$package_no_version" || true
done

if command -v "$CXX" &>/dev/null; then
  $CXX --version
fi

common_install() {

if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    unset -f cd
    echo "macos - set up homebrew openssl"
    export OPENSSL_ROOT=/usr/local/opt/openssl

cat > ~/user-config.jam <<EOF
import os ;
local OPENSSL_ROOT = [ os.environ OPENSSL_ROOT ] ;
project
  : requirements
    <include>/usr/local/opt/openssl/include
    <variant>debug:<library-path>/usr/local/opt/openssl/lib
    <target-os>windows<variant>debug:<library-path>/usr/local/opt/openssl/debug/lib
    <variant>release:<library-path>/usr/local/opt/openssl/lib
  ;
EOF

  fi

  # The name of the current module
  SELF=$(basename "$DRONE_REPO")
  export SELF

  # The boost branch we should clone
  export BOOST_CI_TARGET_BRANCH="$DRONE_BRANCH"

  # The URL source directory to patch boost
  BOOST_CI_SRC_FOLDER=$(pwd)
  export BOOST_CI_SRC_FOLDER
  cache_dir=$BOOST_CI_SRC_FOLDER/cache

  # Number of fetch jobs
  if command -v python &>/dev/null; then
    python_executable="python"
  elif command -v python3 &>/dev/null; then
    python_executable="python3"
  elif command -v python2 &>/dev/null; then
    python_executable="python2"
  else
    echo "Please install Python!" >&2
    false
  fi
  if [ -f "/proc/cpuinfo" ]; then
    GIT_FETCH_JOBS=$(grep -c ^processor /proc/cpuinfo)
  else
    GIT_FETCH_JOBS=$($python_executable -c 'import multiprocessing as mp; print(mp.cpu_count())')
  fi
  export GIT_FETCH_JOBS

  # Determine the final boost branch
  if [ "$BOOST_CI_TARGET_BRANCH" == "master" ] || [[ "$BOOST_CI_TARGET_BRANCH" == */master ]]; then
    export BOOST_BRANCH="master"
  else
    export BOOST_BRANCH="develop"
  fi

  # Boost cache key
  #
  # This key varies every few hours on an update of boost:
  # boost_hash=$(git ls-remote https://github.com/boostorg/boost.git $BOOST_BRANCH | awk '{ print $1 }')
  #
  # This key finds the most recent git tag and will vary every few months:
  boost_hash=$(git ls-remote --tags https://github.com/boostorg/boost | fgrep -v ".beta" | fgrep -v ".rc" | tail -n 1 | cut -f 1)

  os_name=$(uname -s)
  boost_cache_key=$os_name-boost-$boost_hash

  # Validate cache
  if [ ! -d "cache" ]; then
    mkdir "cache"
    boost_cache_hit=false
  else
    if [ -d "cache/boost" ]; then
      if [ -f "cache/boost_cache_key.txt" ]; then
        boost_cached_key=$(cat cache/boost_cache_key.txt)
        if [ "$boost_cache_key" == "$boost_cached_key" ] && [ -f "cache/boost/.gitmodules" ]; then
          boost_cache_hit=true
        else
          echo "boost_cached_key=$boost_cached_key (expected $boost_cache_key)"
          rm -rf "cache/boost"
          boost_cache_hit=false
        fi
      else
        echo "Logic error: cache/boost stored without boost_cache_key.txt"
        rm -rf "cache/boost"
        boost_cache_hit=false
      fi
    else
      boost_cache_hit=false
    fi
  fi

  # Setup boost
  # If no cache: Clone, patch with boost-ci/ci, run common_install, and cache the result
  # If cache hit: Copy boost from cache, patch $SELF, and look for new dependencies with depinst
  # Both paths end at $BOOST_ROOT
  git clone https://github.com/boostorg/boost-ci.git boost-ci-cloned --depth 1
  cp -prf boost-ci-cloned/ci .
  rm -rf boost-ci-cloned
  if [ "$boost_cache_hit" = true ]; then
    cd ..
    mkdir boost-root
    cd boost-root
    BOOST_ROOT="$(pwd)"
    export BOOST_ROOT
    if command -v apt-get &>/dev/null; then
      apt-get install -y rsync
    fi
    rsync -a "$cache_dir/boost/" "$BOOST_ROOT"
    rm -rf "$BOOST_ROOT/libs/$SELF"
    mkdir "$BOOST_ROOT/libs/$SELF"
    cd $DRONE_WORKSPACE
  fi
  . ./ci/common_install.sh

  if [ "$drone_cache_rebuild" == true ]; then
    if command -v apt-get &>/dev/null; then
      apt-get install -y rsync
    fi
    mkdir -p "$cache_dir"/boost
    rsync -a --delete "$BOOST_ROOT/" "$cache_dir/boost" --exclude "$BOOST_ROOT/libs/$SELF/cache"
    # and as a double measure
    rm -rf $cache_dir/boost/libs/$SELF/cache
    echo "$boost_cache_key" >"$cache_dir/boost_cache_key.txt"
  fi
}

if  [ "$DRONE_BEFORE_INSTALL" == "beast_coverage" ]; then
    LCOV_VERSION=${LCOV_VERSION:-1.14}
    pip install --user https://github.com/codecov/codecov-python/archive/master.zip
    wget http://downloads.sourceforge.net/ltp/lcov-$LCOV_VERSION.tar.gz
    tar -xvf lcov-$LCOV_VERSION.tar.gz
    cd lcov-$LCOV_VERSION
    make install
    cd ..
fi

if [ "$DRONE_JOB_BUILDTYPE" == "boost" ]; then

  echo '==================================> BOOST INSTALL'

  common_install

  echo '==================================> SCRIPT'

  # export B2_TARGETS=${B2_TARGETS:-"libs/$SELF/test libs/$SELF/example"}
  if [ -n "$COMPILER" ] && [ -n "$B2_TOOLSET" ]; then
    echo "using $B2_TOOLSET : : $COMPILER ;" >> ~/user-config.jam
  fi

  "$BOOST_ROOT/libs/$SELF/ci/travis/build.sh"

elif [ "$DRONE_JOB_BUILDTYPE" == "boost_v1" ]; then

  # version based on the earlier boost.beast .travis.yml configuration
  
  echo '==================================> INSTALL'
  
  export SELF=`basename $DRONE_REPO`
  export BEAST_RETRY=False
  export TRAVIS=False
  
  BOOST_BRANCH=develop
  if [ "$DRONE_BRANCH" == "master" ]; then
    BOOST_BRANCH=master
  fi
  echo BOOST_BRANCH: $BOOST_BRANCH
  cd ..
  git clone -b $BOOST_BRANCH --depth 1 https://github.com/boostorg/boost.git boost-root
  cd boost-root
  export BOOST_ROOT=$(pwd)
  export PATH=$PATH:$BOOST_ROOT
  # git submodule update --init tools/boostdep
  # python tools/boostdep/depinst/depinst.py --git_args "--jobs 3" $SELF
  git submodule update --init --depth 20 --jobs 4
  rm -rf libs/$SELF
  cp -r $DRONE_WORKSPACE libs/$SELF
  ./bootstrap.sh
  cp libs/beast/tools/user-config.jam ~/user-config.jam
  echo "using $TOOLSET : : $COMPILER : $CXX_FLAGS ;" >> ~/user-config.jam
  # ./b2 -d0 headers
  
  echo '==================================> SCRIPT'
  
  cd $BOOST_ROOT
  libs/beast/tools/retry.sh libs/beast/tools/build-and-test.sh

elif [ "$DRONE_JOB_BUILDTYPE" == "docs" ]; then

  echo '==================================> INSTALL'

  SELF=$(basename "$DRONE_REPO")
  export SELF

  pwd
  cd ..
  mkdir -p "$HOME/cache" && cd "$HOME/cache"
  if [ ! -d doxygen ]; then git clone -b 'Release_1_8_15' --depth 1 https://github.com/doxygen/doxygen.git && echo "not-cached"; else echo "cached"; fi
  cd doxygen
  cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
  cd build
  sudo make install
  cd ../..
  if [ ! -f saxonhe.zip ]; then wget -O saxonhe.zip https://sourceforge.net/projects/saxon/files/Saxon-HE/9.9/SaxonHE9-9-1-4J.zip/download && echo "not-cached"; else echo "cached"; fi
  unzip -o saxonhe.zip
  sudo rm /usr/share/java/Saxon-HE.jar
  sudo cp saxon9he.jar /usr/share/java/Saxon-HE.jar
  cd ..
  BOOST_BRANCH=develop && [ "$DRONE_BRANCH" == "master" ] && BOOST_BRANCH=master || true
  git clone -b $BOOST_BRANCH https://github.com/boostorg/boost.git boost-root --depth 1
  cd boost-root
  BOOST_ROOT=$(pwd)
  export BOOST_ROOT
  git submodule update --init libs/context
  git submodule update --init tools/boostbook
  git submodule update --init tools/boostdep
  git submodule update --init tools/docca
  git submodule update --init tools/quickbook
  rsync -av "$DRONE_WORKSPACE/" "libs/$SELF"
  python tools/boostdep/depinst/depinst.py ../tools/quickbook
  ./bootstrap.sh
  ./b2 headers

  echo '==================================> SCRIPT'

  echo "using doxygen ; using boostbook ; using saxonhe ;" >tools/build/src/user-config.jam
  ./b2 -j3 "libs/$SELF/doc//boostrelease"

elif [ "$DRONE_JOB_BUILDTYPE" == "codecov" ]; then

  echo '==================================> INSTALL'

  common_install

  echo '==================================> SCRIPT'

  set +e

  cd "$BOOST_ROOT/libs/$SELF"
  ci/travis/codecov.sh

  # coveralls
  # uses multiple lcov steps from boost-ci codecov.sh script
  if [ -n "${COVERALLS_REPO_TOKEN}" ]; then
    echo "processing coveralls"
    pip3 install --user cpp-coveralls
    cd "$BOOST_CI_SRC_FOLDER"

    export PATH=/tmp/lcov/bin:$PATH
    command -v lcov
    lcov --version

    lcov --remove coverage.info -o coverage_filtered.info '*/test/*' '*/extra/*'
    cpp-coveralls --verbose -l coverage_filtered.info
  fi

elif [ "$DRONE_JOB_BUILDTYPE" == "valgrind" ]; then

  echo '==================================> INSTALL'

  common_install

  echo '==================================> SCRIPT'

  cd "$BOOST_ROOT/libs/$SELF"
  ci/travis/valgrind.sh

elif [ "$DRONE_JOB_BUILDTYPE" == "coverity" ]; then

  echo '==================================> INSTALL'

  common_install

  echo '==================================> SCRIPT'

  if [ -n "${COVERITY_SCAN_NOTIFICATION_EMAIL}" ] && { [ "$DRONE_BRANCH" = "develop" ] || [ "$DRONE_BRANCH" = "master" ]; } && { [ "$DRONE_BUILD_EVENT" = "push" ] || [ "$DRONE_BUILD_EVENT" = "cron" ]; }; then
    cd "$BOOST_ROOT/libs/$SELF"
    export BOOST_REPO="$DRONE_REPO"
    export BOOST_BRANCH="$DRONE_BRANCH"
    $BOOST_CI_SRC_FOLDER/ci/coverity.sh
  fi

fi
