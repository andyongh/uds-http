#!/bin/bash
set -e

echo "Ensuring required macOS dependencies are installed..."
if command -v brew &> /dev/null; then
  # No longer need libuv
  # brew list libuv &>/dev/null || brew install libuv
  brew list llhttp &>/dev/null || brew install llhttp
  brew list lz4 &>/dev/null || brew install lz4
  echo "Dependencies installed via Homebrew."
else
  echo "Homebrew not found. Please assure llhttp, and lz4 are installed."
fi

mkdir -p src
cd src
if [ ! -f "yyjson.h" ]; then
  echo "Downloading yyjson.h..."
  curl -sSLO https://raw.githubusercontent.com/ibireme/yyjson/master/src/yyjson.h
fi
if [ ! -f "yyjson.c" ]; then
  echo "Downloading yyjson.c..."
  curl -sSLO https://raw.githubusercontent.com/ibireme/yyjson/master/src/yyjson.c
fi
cd ..
echo "Dependencies setup complete!"
