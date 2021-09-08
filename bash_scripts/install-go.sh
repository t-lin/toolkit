#!/bin/bash
DL_PAGE_URL=https://golang.org/dl/
DL_PAGE=`curl -s ${DL_PAGE_URL}`
if [[ -n ${DL_PAGE} ]]; then
    DL_URLS=`echo "${DL_PAGE}" | grep -o -E "go[0-9.]+src.tar.gz"`
    VERSIONS=`echo "${DL_URLS}" | sed -E 's#(go|.src.tar.gz)##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (golang version)"
    echo "Available versions are:" ${VERSIONS}
    exit 1
else
    GOVERS=$1
    if [[ ! "${VERSIONS}" =~ "${GOVERS}" ]]; then
        echo "ERROR: Version ${GOVERS} is not in the list of available versions"
        echo "Available versions are:" ${VERSIONS}
        exit 1
    fi
fi

echo "Fetching golang files..."
WGET_RES=`wget ${DL_PAGE_URL}/go${GOVERS}.linux-amd64.tar.gz -O /tmp/go${GOVERS}.tar.gz 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm /tmp/go${GOVERS}.tar.gz # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download golang version ${GOVERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extract to home directory
echo "Extracting tar file..."
tar -xvf /tmp/go${GOVERS}.tar.gz -C /tmp
rm /tmp/go${GOVERS}.tar.gz # Clean-up

# Move and rename directory w/ version info and create symblink
mv /tmp/go ~/go.${GOVERS}
rm -f ~/go
ln -s ~/go.${GOVERS} ~/go

# GOPATH directory, make it in home for now
mkdir -p ~/gopath

# Update bashrc if either GOPATH or GOROOT is not defined
if [[ -z ${GOPATH} || -z ${GOROOT} ]]; then
    echo "Updating bashrc..."
    cat <<TLINEND >> ~/.bashrc

export GOROOT=\${HOME}/go
export GOPATH=\${HOME}/gopath
export PATH=\${PATH}:\${GOROOT}/bin:\${GOPATH}/bin
TLINEND

    echo
    echo "Done; Re-source bashrc now, or log out and log back in"
fi
