#!/bin/bash

# Yes, /bin/bash, not sh. There are a few bashisms here that I don't
# know how to avoid, e.g. building the params array below.

# To make this work both on the local machine where the emacs is
# running and on remote machines where we've remote-forwarded a port
# through our ssh connection, we first check whether there exists a
# suitable ~/.emacs.d/server/server.

localserverfile="${HOME}/.emacs.d/server/server"
remoteserverfile="${HOME}/.emacs.d/server/remote-server"

if [ -e "${localserverfile}" ] ; then
    pid=$(head -n1 "${localserverfile}" | cut -f2 -d' ')
    if ! grep -q emacs "/proc/${pid}/comm" 2> /dev/null ; then
	echo "local server file ${localserverfile} found but no running emacs with pid ${pid}" > /dev/stderr
	exit 1
    fi
    exec emacsclient "$@"
fi

if [ ! -e "${remoteserverfile}" ] ; then
    echo "neither remote or local server file found" > /dev/stderr
    exit 1
fi

# We explicitly ask for the third line instead of doing tail -n1.
whowhere=$(sed -n '3p' "${remoteserverfile}")

if [ $? -ne 0 -o -z "$whowhere" ] ; then
    echo "failed reading who@where from ${remoteserverfile}" > /dev/stderr
    exit 1
fi

# We need to modify the parameters to emacsclient so that the host's
# emacs knows where to open and find the file, which involves
# prepending a suitable '/ssh:...' prefix and using the file's
# absolute path. But some parameters are options to emacsclient itself
# (e.g. -n), so those need to be passed through as-is.
#
# For now, this unfortunately means that options taking an argument
# must be given using the --opt=val form.

params=()
for p in "$@"; do
    if [[ "$p" =~ ^- ]]; then
        params+=( "$p" )
    # +nn or +nn:mm is used to position point at a specific line/column, so
    # this also needs to be passed through as-is.
    elif [ "${p:0:1}" == "+" ]; then
        params+=( "$p" )
    # Otherwise, it is a filename argument, so we need to modify the
    # argument so that the emacs server can open the file via
    # ssh. Hence we prepend "/ssh:{whoami}@{hostname as the emacs
    # server knows it}:" and change the path to an absolute path.
    else
        params+=( "/ssh:${whowhere}:$(readlink -f -- "$p")" )
    fi
done

exec emacsclient -f "${remoteserverfile}" "${params[@]}"
