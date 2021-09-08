#!/bin/bash
if [[ ! -n `which go` ]]; then
    echo "Make sure Go is installed first"
    exit 1
fi

echo "Installing protoc-gen-go..."
go get google.golang.org/protobuf/cmd/protoc-gen-go

read -p "Clone Google APIs? (May not be needed, but includes useful protobuf libs) [no]: " CLONE
if [[ ${CLONE} =~ ^[yY] ]]; then
    git clone https://github.com/googleapis/googleapis
fi

read -p "Install protoc-gen-go-grpc? (Useful for Go gRPC development) [no]: " CLONE
if [[ ${CLONE} =~ ^[yY] ]]; then
    go get google.golang.org/grpc/cmd/protoc-gen-go-grpc
fi
