if test -f /run/host-services/ssh-auth.sock; then
  sudo chown mpc-user:mpc-user /run/host-services/ssh-auth.sock
fi
. ~/.bashrc
