#!/usr/bin/env python

# Search for the following in FindBoost.cmake
#     message(WARNING "New Boost version may have incorrect or missing dependencies and imported targets")
#     set(_Boost_IMPORTED_TARGETS FALSE)
# and replace it.


from six.moves import urllib # https://stackoverflow.com/a/35039323
import re
import os


url = 'https://gitlab.kitware.com/cmake/cmake/raw/v3.10.0/Modules/FindBoost.cmake'
response = urllib.request.urlopen(url)
data = response.read()      # a `bytes` object
text = data.decode('utf-8') # a `str`; this step can't be used if data is binary



# The set(...) lines below are determined by the following command:
#   cmake -DBOOST_DIR=/my/path/to/boost/BOOST_INSTALL_DIR/include -P /tmp/cmake_dir/Utilities/Scripts/BoostScanDeps.cmake
# where BoostScanDeps.cmake is https://gitlab.kitware.com/cmake/cmake/blob/release/Utilities/Scripts/BoostScanDeps.cmake
# This means that on a new boost, you might need to change this file

deps = '''\
#special tweak for Boost
if (NOT TWEAK_BOOST)
  set(TWEAK_BOOST TRUE CACHE BOOL "")
  message("==> Using tweaked FindBoost.cmake: ${CMAKE_CURRENT_LIST_FILE}")
endif()
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
'''




# text='''\
# AA
#       message(WARNING "New Boost version may have incorrect or missing dependencies and imported targets")
#       set(_Boost_IMPORTED_TARGETS FALSE)
# BB\
# '''

text = re.sub(r'(message\(WARNING\s+"New.*\n)(\s*)(set\(_Boost_IMPORTED_TARGETS\s+FALSE\))',
              r'#\1\2#\3'+"\n"+re.sub("^(.*)", r'\\2\1', deps, flags=re.MULTILINE),
              text);


dir_path = os.path.dirname(os.path.realpath(__file__))
filename = os.path.join(dir_path,
                        url.rsplit('/', 1)[-1] # FindBoost.cmake
                       )

with open(filename, 'w') as f:
    f.write(text)


print("Wrote {}".format(os.path.abspath(filename)))
