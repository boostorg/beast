 #!/usr/bin/env bash

for i in {1..3}
do
    $1 "${@:2:99}" && break;
    export BEAST_RETRY="true"
done
