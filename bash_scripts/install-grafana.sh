#!/bin/bash
if [[ ! -f /etc/apt/sources.list.d/grafana.list ]]; then
    wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
    echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee -a /etc/apt/sources.list.d/grafana.list
fi

sudo apt-get update
sudo apt-get install -y apt-transport-https software-properties-common  grafana

# Enable via systemd and start service
sudo systemctl daemon-reload
sudo systemctl enable grafana-server

echo "Starting service..."
sudo service grafana-server start

