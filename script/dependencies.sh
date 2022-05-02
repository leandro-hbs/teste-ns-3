#!/bin/bash

export DEBIAN_FRONTEND=noninteractive
echo "Installing SQLite 3 and YAML c++ Library"
sudo apt update;
sudo -E apt install -y g++ sqlite3 libsqlite3-dev libyaml-cpp-dev > apt.log 2>&1
