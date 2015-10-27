#!/bin/bash
# touch $HOME/dfasma.touch
export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH; dfasma "$@"
