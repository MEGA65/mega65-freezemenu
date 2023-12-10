#!/bin/bash

# status of 'B'ranch in 'S'hort format
branch=`git rev-parse --abbrev-ref HEAD`
# get from charpos3, for 6 chars
branch2=${branch:0:6}
version=`git describe --always --abbrev=7 | sed -e 's/(//g' -e 's/)//g' -e's/ /_/g'`

datetime=`date +%Y%m%d.%H`
stringout="${datetime}-${branch2}-${version}"
echo $stringout

