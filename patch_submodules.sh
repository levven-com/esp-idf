#!/bin/bash

set -e -x

patch -d components/lwip/lwip -p 1 < lwip.patch
