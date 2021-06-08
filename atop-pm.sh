#!/bin/sh

PATH=/sbin:/usr/sbin:/bin:/usr/bin

case "$1" in
	pre)	systemctl stop atop
		exit 0
		;;
	post)	systemctl start atop
		exit 0
		;;
 	*)	exit 1
		;;
esac
