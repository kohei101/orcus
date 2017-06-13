#!/usr/bin/env bash

# global constants
outdir=xml

function abort()
{
    (>&2 echo "$1")  # output to stderr
    exit 1
}

filepath="$1"

if [ -z $filepath ]; then
    abort "file path is not given."
fi

shift

# convert the file path to absolute path.
filepath=`pwd`/$filepath
filepath=`realpath $filepath`

# remove existing output directory if one exists.
if [ -d $outdir ]; then
    rm -rf $outdir || abort "failed to remove the existing output directory '$outdir'."
fi

mkdir $outdir || abort "failed to create an output directory '$outdir'."

# unzip all inside the output directory.
cd $outdir
unzip $filepath > /dev/null || abort "failed to unzip $filepath."

for _file in `find . -type f`; do
    _mimetype=`file --mime-type --brief $_file` || abort "failed to determine the mime type of $_file."
    if [ $_mimetype = "application/xml" ]; then
        # beautify the XML file content.
        _temp=$(tempfile) || abort "failed to create a temporary file."
        xmllint --format $_file > $_temp || abort "failed to run xmllint on $_file."
        mv $_temp $_file || abort "failed to update $_file."
    fi
done
