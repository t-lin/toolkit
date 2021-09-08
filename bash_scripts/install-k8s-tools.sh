#!/bin/bash
# Installs kubeadm, kubelet, kubectl, and kubernetes-cni
# NOTE: Currently, swap must be disabled for k8s to work

read -p "This script assumes docker-ce is already installed (install now manually), press any key to continue" ASDF

echo "Updating apt and installing pre-reqs..."
sudo apt-get update
sudo apt-get install -y apt-transport-https curl

echo "Setting up custom apt repo..."
curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
sudo bash -c 'echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" > /etc/apt/sources.list.d/kubernetes.list'

echo "Updating apt again and installing kubeadm kubectl kubelet kubernetes-cni..."
sudo apt-get update
sudo apt-get install -y kubelet kubeadm kubectl kubernetes-cni

echo "Enabling IP forwarding to pods"
sudo iptables -P FORWARD ACCEPT

echo "For configuration options, see the rest of this script beneath this line"

#read -p "ACTIVATE MASTER ON THIS NODE? PRESS ANY KEY FOR YES, ELSE CTRL+C NOW" ASDF
#sudo kubeadm init --pod-network-cidr=100.200.0.0/16 --apiserver-advertise-address=10.11.69.6
#mkdir -p $HOME/.kube
#sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
#sudo chown $(id -u):$(id -g) $HOME/.kube/config

#
#read -p "INSTALL FLANNEL AS WELL? PRESS ENTER FOR YES, ELSE CTRL+C NOW"
#
#kubectl apply -f https://raw.githubusercontent.com/coreos/flannel/master/Documentation/kube-flannel.yml
#kubectl apply -f https://raw.githubusercontent.com/coreos/flannel/master/Documentation/k8s-manifests/kube-flannel-rbac.yml

# To run non-management pods within the master node:
#   kubectl taint nodes --all node-role.kubernetes.io/master-

# To test deployment with guid generator:
#   kubectl run guids --image=alexellis2/guid-service:latest --port 9000

