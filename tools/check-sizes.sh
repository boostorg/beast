#!/usr/bin/env bash


(git clone https://github.com/google/bloaty.git \
    && cd bloaty \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j${JOBS}) || exit $?

BLOATY=bloaty/build/bloaty

find $BOOST_ROOT/bin.v2/libs/beast/example -executable -type f | while read file; do
    printf "\n----------------------------------------------------------\n"
    echo "Top 30 largest symbols in example '$file'"
    $BLOATY $file -d cppsymbols -s vm -n 30
    echo "File stats:"
    size $file
    stripped_file=$file'-stripped'
    cp $file $stripped_file
    strip $stripped_file
    echo "Total binary size(with symbols)"
    stat --printf="%s\n" $file
    echo "Total binary size(stripped)"
    stat --printf="%s\n" $stripped_file
    rm $stripped_file
done
