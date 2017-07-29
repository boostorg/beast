#!/usr/bin/env bash
# We use set -e to bail on first non zero exit code of any processes launched
# and -x to exit upon any unbound variable. -x will output command lines used
# (with variable expansion)

BIN_DIR="$BOOST_ROOT/bin.v2/libs/beast/test"
LIB_DIR="$BOOST_ROOT/libs/beast"
INC_DIR="$BOOST_ROOT/boost/beast"

set -eux

# brew install bash (4) to get this working on OSX!
shopt -s globstar

################################## ENVIRONMENT #################################

DO_VALGRIND=${DO_VALGRIND:-false}

# If not CI, then set some defaults
if [[ "${CI:-}" == "" ]]; then
  TRAVIS_BRANCH=${TRAVIS_BRANCH:-feature}
  CC=${CC:-gcc}
  ADDRESS_MODEL=${ADDRESS_MODEL:-64}
  VARIANT=${VARIANT:-debug}
  # If running locally we assume we have lcov/valgrind on PATH
else
  export PATH=$VALGRIND_ROOT/bin:$LCOV_ROOT/usr/bin:$PATH
fi

MAIN_BRANCH="0"
# For builds not triggered by a pull request TRAVIS_BRANCH is the name of the
# branch currently being built; whereas for builds triggered by a pull request
# it is the name of the branch targeted by the pull request (in many cases this
# will be master).
if [[ $TRAVIS_BRANCH == "master" || $TRAVIS_BRANCH == "develop" ]]; then
    MAIN_BRANCH="1"
fi

num_jobs="1"
if [[ $(uname) == "Darwin" ]]; then
  num_jobs=$(sysctl -n hw.physicalcpu)
elif [[ $(uname -s) == "Linux" ]]; then
  # CircleCI returns 32 phys procs, but 2 virt proc
  num_proc_units=$(nproc)
  # Physical cores
  num_jobs=$(lscpu -p | grep -v '^#' | sort -u -t, -k 2,4 | wc -l)
  if ((${num_proc_units} < ${num_jobs})); then
      num_jobs=$num_proc_units
  fi
  if [[ "${TRAVIS}" == "true" && ${NUM_PROCESSORS:=2} > ${num_jobs} ]]; then
      num_jobs=$NUM_PROCESSORS
  fi
  #if [[ "$TRAVIS" == "true" ]] && (( "$num_jobs" > 1)); then
  #    num_jobs=$((num_jobs - 1))
  #fi
fi

echo "using toolset: $TOOLSET"
echo "using variant: $VARIANT"
echo "using address-model: $ADDRESS_MODEL"
echo "using PATH: $PATH"
echo "using MAIN_BRANCH: $MAIN_BRANCH"
echo "using BOOST_ROOT: $BOOST_ROOT"

#################################### HELPERS ###################################

function run_tests_with_debugger {
  for x in $BOOST_ROOT/bin.v2/libs/beast/test/**/$VARIANT/**/fat-tests; do
    "$LIB_DIR/scripts/run-with-debugger.sh" "$x"
  done
}

function run_tests {
  for x in $BOOST_ROOT/bin.v2/libs/beast/test/**/$VARIANT/**/fat-tests; do
    $x
  done
}

function run_tests_with_valgrind {
  for x in $BOOST_ROOT/bin.v2/libs/beast/test/**/$VARIANT/**/fat-tests; do
    # TODO --max-stackframe=8388608
    # see: https://travis-ci.org/vinniefalco/Beast/jobs/132486245
    valgrind --suppressions=$BOOST_ROOT/libs/beast/scripts/valgrind.supp --error-exitcode=1 "$x"
  done
}

function build_bjam {
  if [[ $VARIANT == "coverage" ]]; then
    bjam \
      libs/beast/test/beast/core//fat-tests \
      libs/beast/test/beast/http//fat-tests \
      libs/beast/test/beast/websocket//fat-tests \
      libs/beast/test/beast/zlib//fat-tests \
        toolset=$TOOLSET variant=$VARIANT address-model=$ADDRESS_MODEL -j${num_jobs}
  else
    bjam \
      libs/beast/test//fat-tests \
      libs/beast/bench \
      libs/beast/example \
        toolset=$TOOLSET variant=$VARIANT address-model=$ADDRESS_MODEL -j${num_jobs}
  fi
}

build_bjam

if [[ $VARIANT == "coverage" ]]; then
  # for lcov to work effectively, the paths and includes
  # passed to the compiler should not contain "." or "..".
  # (this runs in $BOOST_ROOT)
  lcov --version
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f
  rm -f "$BOOST_ROOT/*.info"
  lcov --no-external -c -i -d "$BOOST_ROOT" -o baseline.info > /dev/null
  run_tests
  # https://bugs.launchpad.net/ubuntu/+source/lcov/+bug/1163758
  lcov --no-external -c -d "$BOOST_ROOT"  -o testrun.info > /dev/null 2>&1
  lcov -a baseline.info -a testrun.info -o lcov-all.info > /dev/null
  lcov -e "lcov-all.info" "$INC_DIR/*" -o lcov.info > /dev/null
  ~/.local/bin/codecov -X gcov -f lcov.info
  find "$BOOST_ROOT" -name "*.gcda" | xargs rm -f

elif [[ "$DO_VALGRIND" = true ]]; then
  run_tests_with_valgrind

else
  run_tests_with_debugger

fi
