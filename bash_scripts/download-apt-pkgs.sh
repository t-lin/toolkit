#!/bin/bash
# Downloads apt packages and their dependencies
# Credit: https://stackoverflow.com/questions/22008193/how-to-list-download-the-recursive-dependencies-of-a-debian-package
if [[ $# -ne 1 ]]; then
    echo "ERROR: Must specify the name of the package"
    exit 1
else
    PKG_NAME=$1
fi

APT_CACHE_RES=`apt-cache show ${PKG_NAME}`
if [[ ! -n ${APT_CACHE_RES} ]]; then
    echo "ERROR: Package ${PKG_NAME} does not exist in apt"
    exit 1
fi

DL_DIR=${PKG_NAME}-debs
if [[ -d ${DL_DIR} ]]; then
    echo "ERROR: Download destination directory (${DL_DIR}) already exists in current directly"
    echo "       Go to a different directory, or rename the existing destination directory"
    exit 1
fi

mkdir ${DL_DIR} && cd ${DL_DIR}
apt-get download $(apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts --no-breaks --no-replaces --no-enhances ${PKG_NAME} | grep "^\w" | sort -u)

cd - > /dev/null
echo "Done: Packages for ${PKG_NAME} and its dependencies have been downloaded to ${DL_DIR}"

