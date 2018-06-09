#!/bin/bash

# This is for use in long-running automated jobs where at some point
# one subtask needs to "sudo something". If we're running under make
# -j8, it's very likely that the sudo prompt have long vanished (due
# to other jobs happily strolling along) when I return to the xterm to
# check on the progress. So we use zenity to pop up a graphical
# dialogue. But! I don't want it to interrupt me if I'm working on
# some other desktop.
#
# Oooh, nice, $WINDOWID is the wid of the xterm window where I started
# my long-running job. So obviously it's easy to say "Open the zenity
# dialog window on the desktop containing that window, and make sure
# it's on top once I get back to that desktop."
#
# Nah. For one thing, it seems to be impossible to say beforehand that
# one wants a given window to start on a given desktop; they always
# start on whatever happens to be the active one. That alone means we
# need to do the ugly 'start zenity in the background, busypoll wmctrl
# for when a window belonging to that $pid appears'. Obviously, that
# also means it's not possible to completely avoid being disturbed
# momentarily by the appearance of the dialog window.
#
# After figuring out what desktop number of $WINDOWID, and having
# waited for the zenity window to appear, it's relative simple to move
# it to the right desktop. But there doesn't seem to be any way to
# ensure it has focus when we return to that desktop - the best we can
# do is set the "always on top" flag.
#
# To top it all off, note how $WINDOWID is most likely in decimal, but
# wmctrl -l prints window ids in hex. Moreover, converting $WINDOWID
# to hex with 'printf "%x"' is not really enough, since we'd have to
# use "%08x" or something to get the strings to match, and it's all
# completely undocumented. Instead, let gawk chew on both strings and
# compare the numbers.
#
# Jeez.


# The prompt in $1 is part of the sudo/askpass protocol. But there
# might be several sudos in play at more or less the same time, it's
# nice to be told _what_ command will be executed with su rights (in
# normal interactive sudo usage, the user most likely knows...), and
# then maybe deciding not to do a particular one. Fortunately, it
# seems that sudo doesn't clear out its argv strings, so we can
# actually know what it will do by reading /proc/$PPID/cmdline.
prompt="$1"
cmd=$(tr '\0' ' ' < /proc/$PPID/cmdline)

if which zenity > /dev/null 2> /dev/null ; then
    desktop=""
    if [ -n "$WINDOWID" ] ; then
	desktop=$(wmctrl -l | gawk "strtonum(\$1) == strtonum($WINDOWID) {print \$2}")
    fi
    if [ -n "$desktop" ] ; then
	zenity --forms --add-password="$prompt" --text="$cmd" --title "Please enter your pasword" 2> /dev/null &
	pid="$!"
	n=100
	while [ $n -gt 0 ] ; do
	    n=$((n-1))
	    wid=$(wmctrl -p -l | awk "\$3 == $pid {print \$1}")
	    if [ -n "$wid" ] ; then
		wmctrl -i -r "$wid" -t "$desktop"
		wmctrl -i -r "$wid" -b "add,above"
		break
	    fi
	    sleep .001
	done
	wait -n
	exit $?
    fi
fi

# Fall back to reading from the terminal.
sttysetting=$(stty -g)
trap "stty $sttysetting" EXIT
echo "[[[$cmd]]]" >&2
IFS= read -s -p "$prompt" pwd
ret="$?"
printf "\n" >&2
[ "$ret" -eq 0 ] || exit 1
printf "%s\n" "$pwd"
