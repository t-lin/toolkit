#!/bin/bash
# Script for installing NRPE executor in agents
# TODO: Make plugin & nrpe names flexible by passing in parameters

if [[ $# -ne 1 ]]; then
    echo "ERROR: Expects one parameter (controller's IP)"
    echo -e "\te.g. ./install-nrpe-agent.sh 10.20.60.10"
    exit 1
else
    CTRL_IP=$1
fi

echo "Removing any old plugins..."
sudo service nrpe stop
sudo apt-get --purge -y remove nagios-nrpe-server nagios-plugins nagios-plugins-basic nagios-plugins-standard
sudo apt-get --purge autoremove

echo
echo "Installing NRPE for Nagios 4 and associated plugins"

tar -xvf nagios-plugins-2.2.1.tar.gz
tar -xvf nrpe-3.2.1.tar.gz

cd nagios-plugins-2.2.1
./configure
make -j 20
sudo make install

cd -
cd nrpe-3.2.1
./configure
make all -j 20
sudo make install-groups-users
sudo make install-daemon
sudo make install-config
sudo sed -i "/::1/ s/$/,"${CTRL_IP}"/" /usr/local/nagios/etc/nrpe.cfg
sudo sed -i "s/check_hda1/check_disk/g" /usr/local/nagios/etc/nrpe.cfg
MAIN_DISK=`df -h / | tail -n1 | awk '{print $1}'`
sudo sed -i "s/\/dev\/hda1/"${MAIN_DISK//\//\\/}"/" /usr/local/nagios/etc/nrpe.cfg
sudo make install-init
sudo systemctl enable nrpe
sudo service nrpe restart

echo
echo "Done agent NRPE installation"
echo "Verify installation by running \"./check_nrpe -H <AGENT NAME OR IP>\" in the controller's plugin directory"
