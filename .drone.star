# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.

# Configuration for https://cloud.drone.io/.

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.

def main(ctx):
  addon_base = { "apt": { "packages": [ "software-properties-common", "libffi-dev", "libstdc++6", "binutils-gold", "gdb" ] } }

  return [
    linux_cxx("GCC 6.0, Debug + Coverage", "g++-6", packages=" ".join(addon_base["apt"]["packages"]) + " g++-6 libssl-dev", image="ubuntu:16.04", buildtype="boost", environment={ "VARIANT": "beast_coverage", "TOOLSET": "gcc", "COMPILER": "g++-6", "CXXSTD": "14", "DRONE_BEFORE_INSTALL" : "beast_coverage" }, stepenvironment={"CODECOV_TOKEN": {"from_secret": "codecov_token"}}, privileged=True),
    linux_cxx("Default clang++ with libc++", "clang++-libc++", packages=" ".join(addon_base["apt"]["packages"]), image="ubuntu:16.04", buildtype="boost", environment={ "B2_TOOLSET": "clang-7", "B2_CXXSTD": "17,2a", "VARIANT": "debug", "TOOLSET": "clang", "COMPILER": "clang++-libc++", "CXXSTD": "11", "CXX_FLAGS": "<cxxflags>-stdlib=libc++ <linkflags>-stdlib=libc++", "TRAVISCLANG" : "yes" }),
    linux_cxx("GCC Valgrind", "g++", packages=" ".join(addon_base["apt"]["packages"]) + " g++-7 libssl-dev valgrind", image="ubuntu:20.04", buildtype="boost", environment={ "VARIANT": "beast_valgrind", "TOOLSET": "gcc", "COMPILER": "g++", "CXXSTD": "11" }),
    linux_cxx("Default g++", "g++", packages=" ".join(addon_base["apt"]["packages"]), image="ubuntu:16.04", buildtype="boost", environment={ "VARIANT": "release", "TOOLSET": "gcc", "COMPILER": "g++", "CXXSTD": "11" }),
    linux_cxx("GCC 8, C++17, libstdc++, release", "g++-8", packages=" ".join(addon_base["apt"]["packages"]) + " g++-8", image="ubuntu:16.04", buildtype="boost", environment={  "VARIANT": "release", "TOOLSET": "gcc", "COMPILER": "g++-8", "CXXSTD" : "17" }),
    linux_cxx("Clang 3.8, UBasan", "clang++-3.8", packages=" ".join(addon_base["apt"]["packages"]) + " clang-3.8 libssl-dev", llvm_os="precise", llvm_ver="3.8", image="ubuntu:16.04", buildtype="boost", environment={"VARIANT": "beast_ubasan", "TOOLSET": "clang", "COMPILER": "clang++-3.8", "CXXSTD": "11", "UBSAN_OPTIONS": 'print_stacktrace=1', "DRONE_BEFORE_INSTALL": "UBasan" }),
    linux_cxx("docs", "", packages="docbook docbook-xml docbook-xsl xsltproc libsaxonhe-java default-jre-headless flex libfl-dev bison unzip", image="ubuntu:16.04", buildtype="docs", environment={"COMMENT": "docs"})
    ]

# Generate pipeline for Linux platform compilers.
def linux_cxx(name, cxx, cxxflags="", packages="", llvm_os="", llvm_ver="", arch="amd64", image="ubuntu:16.04", buildtype="boost", environment={}, stepenvironment={}, privileged=False):
  environment_global = {
      "CXX": cxx,
      "CXXFLAGS": cxxflags,
      "PACKAGES": packages,
      "LLVM_OS": llvm_os,
      "LLVM_VER": llvm_ver,
      "B2_CI_VERSION": 1,
      # see: http://www.boost.org/build/doc/html/bbv2/overview/invocation.html#bbv2.overview.invocation.properties
      # - B2_ADDRESS_MODEL=64,32
      # - B2_LINK=shared,static
      # - B2_THREADING=threading=multi,single
      "B2_VARIANT" : "release",
      "B2_FLAGS" : "warnings=extra warnings-as-errors=on"
    }
  environment_current=environment_global
  environment_current.update(environment)

  return {
    "name": "Linux %s" % name,
    "kind": "pipeline",
    "type": "docker",
    "trigger": { "branch": [ "master","develop", "drone", "bugfix/*", "feature/*", "fix/*", "pr/*" ] },
    "platform": {
      "os": "linux",
      "arch": arch
    },
    # Create env vars per generation arguments.
    "environment": environment_current,
    "steps": [
      {
        "name": "Everything",
        "image": image,
        "privileged": privileged,
        "environment": stepenvironment,
        "commands": [

          "echo '==================================> SETUP'",
          "uname -a",
          "apt-get -o Acquire::Retries=3 update && DEBIAN_FRONTEND=noninteractive apt-get -y install tzdata && apt-get -o Acquire::Retries=3 install -y sudo software-properties-common wget curl apt-transport-https git cmake make apt-file sudo libssl-dev git build-essential autotools-dev autoconf && rm -rf /var/lib/apt/lists/*",
          "for i in {1..3}; do apt-add-repository ppa:git-core/ppa && break || sleep 2; done",
          "apt-get -o Acquire::Retries=3 update && apt-get -o Acquire::Retries=3 -y install git",
          "echo '==================================> PACKAGES'",
          "./.drone/linux-cxx-install.sh",

          "echo '==================================> INSTALL AND COMPILE'",
          "./.drone/%s-script.sh" % buildtype,
        ]
      }
    ]
  }

def windows_cxx(name, cxx="g++", cxxflags="", packages="", llvm_os="", llvm_ver="", arch="amd64", image="ubuntu:16.04", buildtype="boost", environment={}, privileged=False):
  environment_global = {
      "CXX": cxx,
      "CXXFLAGS": cxxflags,
      "PACKAGES": packages,
      "LLVM_OS": llvm_os,
      "LLVM_VER": llvm_ver,
      "B2_CI_VERSION": 1,
      # see: http://www.boost.org/build/doc/html/bbv2/overview/invocation.html#bbv2.overview.invocation.properties
      # - B2_ADDRESS_MODEL=64,32
      # - B2_LINK=shared,static
      # - B2_THREADING=threading=multi,single
      "B2_VARIANT" : "release",
      "B2_FLAGS" : "warnings=extra warnings-as-errors=on"
    }
  environment_current=environment_global
  environment_current.update(environment)

  return {
    "name": "Windows %s" % name,
    "kind": "pipeline",
    "type": "docker",
    "trigger": { "branch": [ "master","develop", "drone", "bugfix/*", "feature/*", "fix/*", "pr/*" ] },
    "platform": {
      "os": "windows",
      "arch": arch
    },
    # Create env vars per generation arguments.
    "environment": environment_current,
    "steps": [
      {
        "name": "Everything",
        "image": image,
        "privileged": privileged,
        "commands": [
          "echo '==================================> SETUP'",
          "echo '==================================> PACKAGES'",
          "bash.exe ./.drone/windows-msvc-install.sh",

          "echo '==================================> INSTALL AND COMPILE'",
          "bash.exe ./.drone/%s-script.sh" % buildtype,
        ]
      }
    ]
  }
