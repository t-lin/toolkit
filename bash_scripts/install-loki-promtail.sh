#!/bin/bash
DL_PAGE_URL=https://github.com/grafana/loki/releases
REL_API_URL=https://api.github.com/repos/grafana/loki/releases
REL_API_RES=`curl -s ${REL_API_URL}`
if [[ -n ${REL_API_RES} ]]; then
    # NOTE: Restricting version grep to 2.9.x
    DL_OPTIONS=`echo "${REL_API_RES}" | grep -o -E "2.9.[0-9]/loki-linux-amd64.zip"`
    VERSIONS=`echo "${DL_OPTIONS}" | sed -E 's#/loki-linux-amd64.zip##g' | sort -t . -k 2 -V | uniq`
else
    echo "ERROR: Unable to find available versions, (${DL_PAGE_URL}) appears to be down"
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expecting only one argument (Loki version)"
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

echo "Fetching files..."
TMPFILE=/tmp/loki-linux-amd64.zip
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/loki-linux-amd64.zip -O ${TMPFILE} 2>&1`
if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download Loki version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

if [[ -z `which unzip` ]]; then
    sudo apt-get -y install unzip
fi

# Extracting files
echo "Extracting file..."
unzip ${TMPFILE} -d /tmp/
rm ${TMPFILE} # Clean-up

# Repeat fetch & extract for Promtail
TMPFILE=/tmp/promtail-linux-amd64.zip
WGET_RES=`wget ${DL_PAGE_URL}/download/v${VERS}/promtail-linux-amd64.zip -O ${TMPFILE} 2>&1`
if [[ ! "${WGET_RES}" =~ "200 OK" ]]; then
    rm ${TMPFILE} # Clean-up empty file

    echo
    echo -e "ERROR:\tWasn't able to download Promtail version ${VERS}."
    echo -e "\tDouble-check the version number to see if it exists"
    exit 1
fi

unzip ${TMPFILE} -d /tmp/
rm ${TMPFILE} # Clean-up

LOKI_BIN=/tmp/loki-linux-amd64
PROMTAIL_BIN=/tmp/promtail-linux-amd64
if [[ ! -f ${LOKI_BIN} || ! -f ${PROMTAIL_BIN} ]]; then
    echo -e "ERROR:\tExpecting extracted binaries to be at ${LOKI_BIN} and ${PROMTAIL_BIN}, but could not find them"
    echo -e "\tDouble-check to see if it is named something different and edit this script"
    exit 1
fi

# Create config path and database paths
CONFIG_PATH=/etc/loki
DB_PATH=/var/lib/loki
sudo mkdir -p ${CONFIG_PATH} && sudo chown -R `whoami`:`whoami` ${CONFIG_PATH}
sudo mkdir -p ${DB_PATH}

# Create Loki config file if it doesn't exist
if [[ ! -f ${CONFIG_PATH}/loki.yaml ]]; then
    echo "Creating Loki configuration files in ${CONFIG_PATH}/"
    wget https://raw.githubusercontent.com/grafana/loki/release-2.9.x/cmd/loki/loki-local-config.yaml -O ${CONFIG_PATH}/loki.yaml
    sed -i -E 's#/tmp/wal#'${DB_PATH}'/wal#g' ${CONFIG_PATH}/loki.yaml
    sed -i -E 's#/tmp/loki#'${DB_PATH}'#g' ${CONFIG_PATH}/loki.yaml
fi

# Create Promtail config file if it doesn't exist
if [[ ! -f ${CONFIG_PATH}/promtail.yaml ]]; then
    echo "Creating Promtail configuration files in ${CONFIG_PATH}/"

    # Create config file to scrape journal going back 1 week
    cat <<TLINEND >> ${CONFIG_PATH}/promtail.yaml
server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: ${DB_PATH}/promtail-positions.yaml

clients:
  - url: http://localhost:3100/loki/api/v1/push

scrape_configs:
  - job_name: journal
    journal:
      json: false
      max_age: 168h
      path: /var/log/journal
      labels:
        job: systemd-journal
    relabel_configs:
      - source_labels: ['__journal__systemd_unit']
        target_label: 'unit'
      - source_labels: ['__journal__hostname']
        target_label: 'hostname'
TLINEND
fi

LOCAL_BIN_PATH=/usr/local/bin

# Create Loki systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/loki.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=Loki
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
TimeoutStopSec=30
ExecStart=${LOCAL_BIN_PATH}/loki-linux-amd64 \\
    --config.file ${CONFIG_PATH}/loki.yaml \\
ExecStop=/bin/kill -TERM \$MAINPID

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE}
    sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping Loki service..."
    sudo service loki stop
fi

# Create Promtail systemd service file if it doesn't exist
SYSTEMD_SERVICE=/lib/systemd/system/promtail.service
if [[ ! -f ${SYSTEMD_SERVICE} ]]; then
    TMP_SERVICE=`mktemp`
    cat <<TLINEND >> ${TMP_SERVICE}
[Unit]
Description=Promtail
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
TimeoutStopSec=30
ExecStart=${LOCAL_BIN_PATH}/promtail-linux-amd64 \\
    --config.file ${CONFIG_PATH}/promtail.yaml \\
ExecStop=/bin/kill -TERM \$MAINPID

[Install]
WantedBy=multi-user.target
TLINEND

    # Move to final location
    sudo mv ${TMP_SERVICE} ${SYSTEMD_SERVICE}
    sudo chmod og+r ${SYSTEMD_SERVICE}
else
    # Previous installation may exist, stop it first
    echo "Stopping Promtail service..."
    sudo service promtail stop
fi

# Move/replace binaries
sudo mv ${LOKI_BIN} ${LOCAL_BIN_PATH}
sudo mv ${PROMTAIL_BIN} ${LOCAL_BIN_PATH}

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable loki
sudo systemctl enable promtail

echo "Starting Loki and Promtail services..."
sudo service loki start
sudo service promtail start

