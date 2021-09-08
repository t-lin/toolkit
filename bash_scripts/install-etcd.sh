#!/bin/bash
DL_PAGE_URL=https://github.com/etcd-io/etcd/releases
REL_API_URL=https://api.github.com/repos/etcd-io/etcd/releases
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "etcd-v[0-9.]+-linux-amd64.tar.gz"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#(etcd-v|.linux-amd64.tar.gz)##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${REL_API_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (etd version)"
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

echo "Fetching etcd files..."
TMPFILE=/tmp/etcd-${VERS}.tar.gz
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/etcd-v${VERS}-linux-amd64.tar.gz -O ${TMPFILE} 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download etcd version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extracting files
TMP_DIR=/tmp/etcd-${VERS}
rm -rf ${TMP_DIR} # In case it already exists
mkdir -p ${TMP_DIR}

echo "Extracting tar file..."
tar -xvf ${TMPFILE} -C ${TMP_DIR} --strip-components=1
rm ${TMPFILE} # Clean-up tar file

# Create etcd database path
DB_PATH=/var/lib/etcd
sudo mkdir -p ${DB_PATH}

# Copy/replace binaries
LOCAL_BIN_PATH=/usr/local/bin
sudo cp -a ${TMP_DIR}/etcd* ${LOCAL_BIN_PATH}/

# Create systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/etcd.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=etcd
Documentation=https://github.com/coreos/etcd
Conflicts=etcd.service
Wants=network-online.target
After=network-online.target

[Service]
Type=notify
Restart=always
RestartSec=5s
LimitNOFILE=40000
TimeoutStartSec=0
TimeoutStopSec=30
ExecStart=${LOCAL_BIN_PATH}/etcd --name etcd \
  --data-dir ${DB_PATH} \
  --listen-client-urls http://localhost:2379 \
  --advertise-client-urls http://localhost:2379 \
  --listen-peer-urls http://localhost:2380 \
  --initial-advertise-peer-urls http://localhost:2380 \
  --initial-cluster etcd=http://localhost:2380 \
  --initial-cluster-token tkn \
  --initial-cluster-state new

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE} && sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping service..."
    sudo service etcd stop
fi

# Clean-up
sudo rm -rf ${TMP_DIR}

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable etcd

echo "Starting service..."
sudo service etcd start

