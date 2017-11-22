#!/usr/bin/env bash

# The set(...) lines below are determined by the following command:
#   cmake -DBOOST_DIR=/my/path/to/boost/BOOST_INSTALL_DIR/include -P /tmp/cmake_dir/Utilities/Scripts/BoostScanDeps.cmake
# where BoostScanDeps.cmake is https://gitlab.kitware.com/cmake/cmake/blob/release/Utilities/Scripts/BoostScanDeps.cmake
# This means that on a new boost, you might need to change this file

rm -f FindBoost.cmake
wget https://gitlab.kitware.com/cmake/cmake/raw/release/Modules/FindBoost.cmake


DEPS="#set(_Boost_IMPORTED_TARGETS FALSE)
    # ...set for new boost...
    set(_Boost_CONTEXT_DEPENDENCIES thread chrono system date_time)
    set(_Boost_COROUTINE_DEPENDENCIES context system)
    set(_Boost_FIBER_DEPENDENCIES context thread chrono system date_time)
    set(_Boost_FILESYSTEM_DEPENDENCIES system)
    set(_Boost_IOSTREAMS_DEPENDENCIES regex)
    set(_Boost_LOG_DEPENDENCIES date_time log_    setup system filesystem thread regex chrono atomic)
    set(_Boost_MATH_DEPENDENCIES math_c99 math_c99f math_c99l math_tr1 math_tr1f math_tr1l atomic)
    set(_Boost_MPI_DEPENDENCIES serialization)
    set(_Boost_MPI_PYTHON_DEPENDENCIES python mpi serialization)
    set(_Boost_NUMPY_DEPENDENCIES python)
    set(_Boost_RANDOM_DEPENDENCIES system)
    set(_Boost_THREAD_DEPENDENCIES chrono system date_time atomic)
    set(_Boost_WAVE_DEPENDENCIES filesystem system serialization thread chrono date_time atomic)
    set(_Boost_WSERIALIZATION_DEPENDENCIES serialization)
"

DEPS="${DEPS//$'\n'/\\n}"

echo $DEPS

sed -i "s/set(_Boost_IMPORTED_TARGETS FALSE)/$DEPS/g" FindBoost.cmake
