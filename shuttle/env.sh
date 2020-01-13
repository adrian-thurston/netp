#!/bin/bash
#

ip netns exec ve0 bash -c "sudo -E -u thurston $*"
