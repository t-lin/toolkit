#!/bin/bash
# NOTE: This script assumes we're downlodaing the toolchain for 32-bit BM target
#       ARM calls this toolchain "arm-none-eabi".
#       For 64-bit BM target, use "arm-none-elf"

# Name of item we're downloading & installing
NAME="ARM GNU Toolchain"

DL_PAGE_URL=https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
REL_API_URL=${DL_PAGE_URL}
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "arm-gnu-toolchain-[0-9a-z.]+-$(uname -m)-arm-none-eabi.tar.xz"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#(arm-gnu-toolchain-|-'$(uname -m)'-arm-none-eabi.tar.xz)##g' | sort -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (${NAME} version)"
    echo "Latest versions are:" ${VERSIONS}
    exit 1
else
    VERS=$1
    if [[ ! "${VERSIONS}" =~ "${VERS}" ]]; then
        echo "ERROR: Version ${VERS} is not in the list of available versions"
        echo "Available versions are:" ${VERSIONS}
        exit 1
    fi
fi

echo "Fetching ${NAME} files... (may take a while depending on download speed)"
TMPFILE=/tmp/arm-gnu-toolchain-${VERS}.tar.xz
WGET_RES=`wget https://developer.arm.com/-/media/Files/downloads/gnu/${VERS}/binrel/arm-gnu-toolchain-${VERS}-$(uname -m)-arm-none-eabi.tar.xz -O ${TMPFILE} 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download ${NAME} version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extracting files
echo "Extracting tar file..."
tar -xvf ${TMPFILE} -C /tmp
rm ${TMPFILE} # Clean-up

# Move and rename directory w/ version info and create symblink
TARG_DIR=/opt/arm-gnu-toolchain
sudo mv /tmp/arm-gnu-toolchain-${VERS}-$(uname -m)-arm-none-eabi ${TARG_DIR}-${VERS}
sudo rm -f ${TARG_DIR}
sudo ln -s ${TARG_DIR}-${VERS} ${TARG_DIR}

# Update bashrc if directory isn't in PATH
if [[ -z $(echo ${PATH} | grep ${TARG_DIR}) ]]; then
  echo
  echo "Updating bashrc..."
  cat <<TLINEND >> ~/.bashrc

export PATH=\${PATH}:${TARG_DIR}/bin
TLINEND

  echo
  echo "Done; Re-source bashrc now, or log out and log back in"
fi
