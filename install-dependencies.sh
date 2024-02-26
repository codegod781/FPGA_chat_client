#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root (sudo) for installation priveleges"
  exit
fi


apt update
apt install -y gcc make libusb-1.0-0-dev usbutils openssh-client wget

echo "Done!"
