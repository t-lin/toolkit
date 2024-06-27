#!/bin/bash
DL_PAGE_URL=https://github.com/ncabatoff/process-exporter/releases
REL_API_URL=https://api.github.com/repos/ncabatoff/process-exporter/releases
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "process-exporter-[0-9.]+linux-amd64.tar.gz"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#(process-exporter-|.linux-amd64.tar.gz)##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (Process Exporter version)"
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

echo "Fetching Process Exporter files..."
TMPFILE=/tmp/process-exporter-${VERS}.tar.gz
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/process-exporter-${VERS}.linux-amd64.tar.gz -O ${TMPFILE} 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download Process Exporter version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extracting files
echo "Extracting tar file..."
tar -xvf ${TMPFILE} -C /tmp
rm ${TMPFILE} # Clean-up

TMP_UNTAR_DIR=/tmp/process-exporter-${VERS}.linux-amd64

# Create config path
CONFIG_PATH=/etc/process-exporter
sudo mkdir -p ${CONFIG_PATH}

# Create config file if it doesn't exist
if [[ ! -f ${CONFIG_PATH}/process-exporter.yml ]]; then
    echo "Creating configuration files in ${CONFIG_PATH}/"
    TMPFILE=`mktemp`
    cat <<TLINEND >> ${TMPFILE}
process_names:
  - name: "{{.Comm}}"
    cmdline:
    - '.+'
TLINEND

    # Move to final location
    sudo mv ${TMPFILE} ${CONFIG_PATH}/process-exporter.yml
fi

LOCAL_BIN_PATH=/usr/local/bin

# Create systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/process-exporter.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=Process Exporter
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
TimeoutStopSec=30
ExecStart=${LOCAL_BIN_PATH}/process-exporter -config.path ${CONFIG_PATH}/process-exporter.yml
ExecStop=/bin/kill -TERM \$MAINPID

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE} && sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping service..."
    sudo service process-exporter stop
fi

# Move/replace binaries
sudo mv ${TMP_UNTAR_DIR}/process-exporter ${LOCAL_BIN_PATH}

# Clean-up
sudo rm -rf ${TMP_UNTAR_DIR}

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable process-exporter

echo "Starting service..."
sudo service process-exporter start

