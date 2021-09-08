#!/bin/bash
echo "Removing any old docker..."
sudo apt-get remove -y docker docker-engine docker.io

echo "Updating apt and installing pre-reqs..."
sudo apt-get update
sudo apt-get install -y apt-transport-https ca-certificates curl software-properties-common

echo "Setting up custom apt repo..."
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"

echo "Updating apt again and installing docker-ce..."
sudo apt-get update
sudo apt-get install -y docker-ce

echo "Verifying Docker installation..."
sudo docker run hello-world

echo "Adding ${USER} to docker group (avoids needing to type \"sudo\")"
sudo usermod -aG docker $USER

echo
echo "Please exit and log-in again for usermod changes to take effect"
