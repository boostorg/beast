# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.
#
#
globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}

def main(ctx):

  alljobs=[]
  customizedjobs = [
  linux_cxx("GCC 10, Debug + Coverage", "g++-10", packages="g++-10 libssl-dev libffi-dev binutils-gold gdb mlocate", image="cppalliance/droneubuntu2004:1", buildtype="boost_v1", buildscript="drone", environment={"GCOV": "gcov-10", "LCOV_VERSION": "1.15", "VARIANT": "beast_coverage", "TOOLSET": "gcc", "COMPILER": "g++-10", "CXXSTD": "14", "DRONE_BEFORE_INSTALL" : "beast_coverage", "CODECOV_TOKEN": {"from_secret": "codecov_token"}}, globalenv=globalenv, privileged=True),
  linux_cxx("Default clang++ with libc++", "clang++-libc++", packages="libc++-dev mlocate", image="cppalliance/droneubuntu1604:1", buildtype="boost_v1", buildscript="drone", environment={ "B2_TOOLSET": "clang-7", "B2_CXXSTD": "17,2a", "VARIANT": "debug", "TOOLSET": "clang", "COMPILER": "clang++-libc++", "CXXSTD": "11", "CXX_FLAGS": "<cxxflags>-stdlib=libc++ <linkflags>-stdlib=libc++", "TRAVISCLANG" : "yes" }, globalenv=globalenv),
  linux_cxx("GCC Valgrind", "g++", packages="g++-7 libssl-dev valgrind mlocate", image="cppalliance/droneubuntu2004:1", buildtype="boost_v1", buildscript="drone", environment={ "VARIANT": "beast_valgrind", "TOOLSET": "gcc", "COMPILER": "g++", "CXXSTD": "11" }, globalenv=globalenv),
  linux_cxx("Default g++", "g++", packages="mlocate", image="cppalliance/droneubuntu1604:1", buildtype="boost_v1", buildscript="drone", environment={ "VARIANT": "release", "TOOLSET": "gcc", "COMPILER": "g++", "CXXSTD": "11" }, globalenv=globalenv),
  linux_cxx("GCC 8, C++17, libstdc++, release", "g++-8", packages="g++-8 mlocate", image="cppalliance/droneubuntu1604:1", buildtype="boost_v1", buildscript="drone", environment={  "VARIANT": "release", "TOOLSET": "gcc", "COMPILER": "g++-8", "CXXSTD" : "17" }, globalenv=globalenv),
  linux_cxx("Clang 14, UBasan", "clang++-14", packages="clang-14 libssl-dev mlocate", image="cppalliance/droneubuntu2204:1", buildtype="boost_v1", buildscript="drone", environment={"VARIANT": "beast_ubasan", "TOOLSET": "clang", "COMPILER": "clang++-14", "CXXSTD": "17", "UBSAN_OPTIONS": 'print_stacktrace=1', "DRONE_BEFORE_INSTALL": "UBasan" }, globalenv=globalenv),
  linux_cxx("docs", "", packages="docbook docbook-xml docbook-xsl xsltproc libsaxonhe-java default-jre-headless flex libfl-dev bison unzip rsync mlocate", image="cppalliance/droneubuntu1804:1", buildtype="docs", buildscript="drone", environment={"COMMENT": "docs"}, globalenv=globalenv),
  linux_cxx("ubsan g++-12", "g++-12", packages="g++-12", buildtype="boost", buildscript="drone", image="cppalliance/droneubuntu2204:1", environment={'COMMENT': 'ubsan', 'B2_VARIANT': 'debug', 'B2_TOOLSET': 'gcc-12', 'B2_CXXSTD': '14,17,20', 'B2_UBSAN': '1', 'B2_DEFINES': 'BOOST_NO_STRESS_TEST=1', 'B2_LINKFLAGS': '-fuse-ld=gold'}, globalenv=globalenv),
  linux_cxx("coverity", "g++", packages="", buildtype="coverity", buildscript="drone", image="cppalliance/droneubuntu1804:1", environment={'COMMENT': 'Coverity Scan', 'B2_TOOLSET': 'clang', 'DRONE_JOB_UUID': '472b07b9fc'}, globalenv=globalenv),
  windows_cxx("msvc-14.1", "", image="cppalliance/dronevs2017", buildtype="boost", buildscript="drone", environment={ "VARIANT": "release", "TOOLSET": "msvc-14.1", "CXXSTD": "17", "ADDRESS_MODEL": "64"}),
  windows_cxx("msvc-14.2", "", image="cppalliance/dronevs2019", buildtype="boost", buildscript="drone", environment={ "VARIANT": "release", "TOOLSET": "msvc-14.2", "CXXSTD": "17", "ADDRESS_MODEL": "64"}),
  windows_cxx("msvc-14.3", "", image="cppalliance/dronevs2022:1", buildtype="boost", buildscript="drone", environment={ "VARIANT": "release", "TOOLSET": "msvc-14.3", "CXXSTD": "20", "ADDRESS_MODEL": "64"}),
   ]

  generatedjobs = generate(
        # Compilers
        ['gcc >=5.0',
         'clang >=3.8',
         # 'msvc >=14.1',
         'arm64-gcc latest',
         's390x-gcc latest',
         'freebsd-gcc latest',
         'apple-clang latest',
         'arm64-clang latest',
         # 's390x-clang latest',
         'freebsd-clang latest',
         # 'x86-msvc latest'
        ],
        # Standards
        '>=11',
        docs=False, ubsan=False, asan=False, cache_dir='cache'
)

  alljobs.extend(generatedjobs)
  alljobs.extend(customizedjobs)
  return alljobs

# from https://github.com/boostorg/boost-ci
load("@ci_automation//ci/drone/:functions.star", "linux_cxx","windows_cxx","osx_cxx","freebsd_cxx", "generate")
