#!/bin/bash

shorten_name () {
  name=$1
  # issue branch?
  if [[ $name =~ ^([0-9]+)-(.+)$ ]]; then
    name="${BASH_REMATCH[1]}${BASH_REMATCH[2]}"
  fi
  # developemnt + extra?
  if [[ $name =~ ^development-(.+)$ ]]; then
    name="dev${BASH_REMATCH[1]}"
  fi
  # release + version?
  if [[ $name =~ ^release-([0-9]\.[0-9]\.[0-9])$ ]]; then
    name="r${BASH_REMATCH[1]}"
  fi
  # cut after 6 chars
  echo ${name:0:6}
}

# status of 'B'ranch in 'S'hort format
branch=`git branch --remote --verbose --no-abbrev --contains | sed -rne 's/^[^\/]*\/([^\ ]+).*$/\1/p'`
branch2=$(shorten_name $branch)
version=`git describe --always --abbrev=7 | sed -e 's/(//g' -e 's/)//g' -e's/ /_/g'`

datetime=`date +%Y%m%d.%H`
stringout="${datetime}-${branch2}-${version}"
echo $stringout

