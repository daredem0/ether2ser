#!/usr/bin/env bash
set -euo pipefail

DEST_IP="192.168.29.20"
DEST_PORT="6969"

echo "Type a line and press Enter to send via UDP to ${DEST_IP}:${DEST_PORT} (Ctrl+C to quit)"
hostname=$(hostnamectl --static)
user=$(whoami)

while true; do
  # Read one line from the terminal (preserves leading/trailing spaces with IFS=)
  IFS= read -r line || break

  # Send as UDP datagram
  printf "FROM $user@$hostname: %s\n" "$line" | socat - "UDP-DATAGRAM:${DEST_IP}:${DEST_PORT},broadcast"
done
