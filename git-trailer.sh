#!/bin/bash

set -e

# git-trailer.sh [-i] <trailers>
#
# For example,
#
#   git-trailer.sh "Reviewed-by: Some Dude <sd@example.com>" "Acked-by: Some Other Dude <sod@example.org>"
#
# This will amend the current commit with the given trailers. Some
# common *-by can be given using an abbreviation:
#
# ack: Some Dude -> Acked-by: Some Dude
# rev: Some Dude -> Reviewed-by: Some Dude
# rep: Some Dude -> Reported-by: Some Dude
# test: Some Dude -> Tested-by: Some Dude
#
# Option -i means interactive. You will be asked for each trailer if
# you want to append it or not. This can be useful when used in a "git
# rebase -x" situtation, where not every tag necessarily applies to
# all patches (e.g. Some Other Dude may only have acked patches 2-7).
#

interactive=""

while [[ $# -gt 0 ]] ; do
    case "$1" in
	-i) interactive=1 ; shift ;;
	*) break ;;
    esac
done

trailers=()
while [[ $# -gt 0 ]] ; do
    t="$1"
    case "$t" in
	ack:*) t="Acked-by${t#ack}" ;;
	rep:*) t="Reported-by${t#rep}" ;;
	rev:*) t="Reviewed-by${t#rev}" ;;
	test:*) t="Tested-by${t#test}" ;;
    esac
    trailers+=("$t")
    shift
done

msg_file=$(mktemp)
trap 'rm -f $msg_file' EXIT

git log -1 --pretty=%B > "${msg_file}"
subject=$(head -n1 "${msg_file}")
added=0

for t in "${trailers[@]}"; do
    if [ -n "$interactive" ] ; then
	read -p "Add trailer '$t' to commit '${subject}' (Y/n/a)? "
	case "${REPLY}" in
	    y*|Y*|"") : ;;
	    n*|N*)    continue ;;
	    a*|A*)    interactive="" ;;
	esac
    fi
    added=$((added + 1))
    git interpret-trailers --in-place --if-exists addIfDifferent --trailer "$t" "${msg_file}"
done

if [[ $added -gt 0 ]] ; then
    git commit --amend -F "${msg_file}" --cleanup=verbatim
fi
