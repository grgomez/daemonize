#!/bin/bash
#-----------------------------------------------------------
# German R. Gomez Urbina
# Daemonize init script
#
# Initialize all necessary sub-systems to start the daemon
#-----------------------------------------------------------

echo "Starting rsyslog service"
sudo service rsyslog start

echo "Starting daemon"
sudo /home/german/source/daemonize/daemonize

echo "Done!"
