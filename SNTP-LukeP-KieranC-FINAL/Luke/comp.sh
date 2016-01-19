#!/usr/bin/bash
if gcc -Wall client-full.c externResource.c -o client; then
    echo "Holy Shit it worked"
else
    echo "Ahhh, close but no cigar"
fi
