#!/bin/bash

SSH_USER=iseki
SSH_PASS=*******
SSH_HOST=202.26.144.41
CMD="getent passwd"

if [ -n "$PASSWORD" ]; then
  echo "$PASSWORD"
  exit 0
fi

export PASSWORD=$SSH_PASS
export SSH_ASKPASS=$0
export DISPLAY=:0.0

setsid ssh $SSH_USER@$SSH_HOST $CMD 
#exec setsid ssh $SSH_USER@$SSH_HOST $CMD | get_valid_user.sh
