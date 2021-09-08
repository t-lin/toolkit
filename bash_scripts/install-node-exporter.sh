#!/bin/bash
DL_PAGE_URL=https://github.com/prometheus/node_exporter/releases
REL_API_URL=https://api.github.com/repos/prometheus/node_exporter/releases
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "node_exporter-[0-9.]+linux-amd64.tar.gz"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#(node_exporter-|.linux-amd64.tar.gz)##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (Node Exporter version)"
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

echo "Fetching Node Exporter files..."
TMPFILE=/tmp/node_exporter-${VERS}.tar.gz
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/node_exporter-${VERS}.linux-amd64.tar.gz -O ${TMPFILE} 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download Node Exporter version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extracting files
echo "Extracting tar file..."
tar -xvf ${TMPFILE} -C /tmp
rm ${TMPFILE} # Clean-up

TMP_UNTAR_DIR=/tmp/node_exporter-${VERS}.linux-amd64

# Create config path
CONFIG_PATH=/etc/node_exporter
sudo mkdir -p ${CONFIG_PATH}

# Create config file if it doesn't exist
if [[ ! -f ${CONFIG_PATH}/node_exporter.conf ]]; then
    echo "Creating configuration files in ${CONFIG_PATH}/"
    TMPFILE=`mktemp`
    cat <<TLINEND >> ${TMPFILE}
#OPTIONS=--web.listen-address=":9100" --web.telemetry-path="/metrics"
TLINEND

    # Move to final location
    sudo mv ${TMPFILE} ${CONFIG_PATH}/node_exporter.conf
fi

LOCAL_BIN_PATH=/usr/local/bin

# Create systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/node-exporter.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=Node Exporter
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
TimeoutStopSec=30
EnvironmentFile=${CONFIG_PATH}/node_exporter.conf
ExecStart=${LOCAL_BIN_PATH}/node_exporter \$OPTIONS
ExecStop=/bin/kill -TERM \$MAINPID

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE} && sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping service..."
    sudo service node-exporter stop
fi

# Move/replace binaries
sudo mv ${TMP_UNTAR_DIR}/node_exporter ${LOCAL_BIN_PATH}

# Clean-up
sudo rm -rf ${TMP_UNTAR_DIR}

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable node-exporter

echo "Starting service..."
sudo service node-exporter start

