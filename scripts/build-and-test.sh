#!/bin/bash -u
# We use set -e and bash with -u to bail on first non zero exit code of any
# processes launched or upon any unbound variable. -x will output command lines
# used (with variable expansion)
shopt -s globstar
set -ex

################################## ENVIRONMENT #################################

export PATH=$VALGRIND_ROOT/bin:$LCOV_ROOT/usr/bin:$PATH

# TODO:
MAIN_BRANCH="0"
# For builds not triggered by a pull request TRAVIS_BRANCH is the name of the
# branch currently being built; whereas for builds triggered by a pull request
# it is the name of the branch targeted by the pull request (in many cases this
# will be master).
if [[ $TRAVIS_BRANCH == "master" || $TRAVIS_BRANCH == "develop" ]]; then
    MAIN_BRANCH="1"
fi

echo "using toolset: $CC"
echo "using variant: $VARIANT"
echo "using address-model: $ADDRESS_MODEL"
echo "using PATH: $PATH"
echo "using MAIN_BRANCH: $MAIN_BRANCH"

#################################### HELPERS ###################################

function run_tests_with_gdb {
  for x in bin/**/*-tests; do scripts/run-with-gdb.sh "$x"; done
}

function build_beast {
  $BOOST_ROOT/bjam toolset=$CC \
               variant=$VARIANT \
               address-model=$ADDRESS_MODEL
}

function run_autobahn_test_suite {
  # Run autobahn tests
  wsecho=`find . -name "websocket-echo"`
  nohup scripts/run-with-gdb.sh $wsecho&

  # We need to wait a while so wstest can connect!
  sleep 5
  cd scripts && wstest -m fuzzingclient
  cd ..
  # Show the output
  cat nohup.out
  rm nohup.out
  jobs
  # Kill it gracefully
  kill -INT %1
  # Sometimes it doesn't want to die
  sleep 1
  kill -INT %1 || echo "Dead already"
}

##################################### BUILD ####################################

build_beast

##################################### TESTS ####################################

if [[ $VARIANT == "coverage" ]]; then
  find . -name "*.gcda" | xargs rm -f
  rm *.info -f
  # Create baseline coverage data file
  lcov --no-external -c -i -d . -o baseline.info > /dev/null

  # Perform test
  run_tests_with_gdb

  if [[ $MAIN_BRANCH == "1" ]]; then
    run_autobahn_test_suite
  fi

  # Create test coverage data file
  lcov --no-external -c -d . -o testrun.info > /dev/null

  # Combine baseline and test coverage data
  lcov -a baseline.info -a testrun.info -o lcov-all.info > /dev/null

  # Extract only include/beast, and don\'t report on examples/test
  lcov -e "lcov-all.info" "$PWD/include/beast/*" -o lcov.info > /dev/null

  ~/.local/bin/codecov -X gcov
  cat lcov.info | node_modules/.bin/coveralls
else
  # TODO: make a function
  run_tests_with_gdb

  if [[ $MAIN_BRANCH == "1" ]] && [[ $VARIANT == "coverage" ]]; then
    for x in bin/**/*-tests; do
      # if [[ $x != "bench-tests" ]]; then
      valgrind --error-exitcode=1 "$x"
        ## declare -i RESULT=$RESULT + $?
      # fi
    done
    echo
  fi
fi
