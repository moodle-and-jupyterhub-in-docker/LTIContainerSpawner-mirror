# .bash_profile

# Get the aliases and functions
if [ -f ~/.bashrc ]; then
	. ~/.bashrc
fi

# User specific environment and startup programs
stty dec pass8 cs8 erase '^?'
LANG=ja_JP.UTF-8

if [ "$TERM" = "linux" ]; then
    LANG=C
fi

# addpath $HOME/bin
BASH_ENV=$HOME/.bashrc
PKG_CONFIG_PATH=/usr/lib/pkgconfig/
USERNAME=""

export LANG USERNAME BASH_ENV PATH LESSOPEN PKG_CONFIG_PATH

