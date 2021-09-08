#!/bin/bash
DL_PAGE_URL=https://github.com/prometheus/prometheus/releases
REL_API_URL=https://api.github.com/repos/prometheus/prometheus/releases
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "prometheus-[0-9.]+linux-amd64.tar.gz"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#(prometheus-|.linux-amd64.tar.gz)##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (Prometheus version)"
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

echo "Fetching Prometheus files..."
TMPFILE=/tmp/prometheus-${VERS}.tar.gz
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/prometheus-${VERS}.linux-amd64.tar.gz -O ${TMPFILE} 2>&1`

if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download Prometheus version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

# Extracting files
echo "Extracting tar file..."
tar -xvf ${TMPFILE} -C /tmp
rm ${TMPFILE} # Clean-up

TMP_DIR=/tmp/prometheus-${VERS}.linux-amd64

# Create Prometheus config path and database paths
CONFIG_PATH=/etc/prometheus
DB_PATH=/var/lib/prometheus
sudo mkdir -p ${CONFIG_PATH}
sudo mkdir -p ${DB_PATH}

# Copy/replace config dirs
# Overwrite web portal files; Create config file if it doesn't exist
sudo cp -ar ${TMP_DIR}/consoles ${CONFIG_PATH}
sudo cp -ar ${TMP_DIR}/console_libraries ${CONFIG_PATH}
if [[ ! -f ${CONFIG_PATH}/prometheus.yml ]]; then
    echo "Creating configuration files in ${CONFIG_PATH}/"
    sudo cp -a ${TMP_DIR}/prometheus.yml ${CONFIG_PATH}
fi

LOCAL_BIN_PATH=/usr/local/bin

# Create systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/prometheus.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=Prometheus
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
TimeoutStopSec=30
ExecStart=${LOCAL_BIN_PATH}/prometheus \\
    --config.file ${CONFIG_PATH}/prometheus.yml \\
    --storage.tsdb.path ${DB_PATH}/ \\
    --web.console.templates=${CONFIG_PATH}/consoles \\
    --web.console.libraries=${CONFIG_PATH}/console_libraries
ExecStop=/bin/kill -TERM \$MAINPID
ExecReload=/bin/kill -HUP \$MAINPID

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE} && sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping service..."
    sudo service prometheus stop
fi

# Move/replace binaries
sudo mv ${TMP_DIR}/prometheus ${LOCAL_BIN_PATH}
sudo mv ${TMP_DIR}/promtool ${LOCAL_BIN_PATH}

# Clean-up
sudo rm -rf ${TMP_DIR}

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable prometheus

echo "Starting service..."
sudo service prometheus start

