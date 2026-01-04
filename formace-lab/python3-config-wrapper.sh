#!/bin/bash
# Wrapper for python3-config using conda's version
exec $CONDA_PREFIX/bin/python3-config "$@"


