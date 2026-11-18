#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec autoreconf --force --install
