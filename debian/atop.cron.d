PATH=/bin:/usr/bin:/sbin:/usr/sbin

# daily restart of atop at midnight
0 0 * * * root [ -d "/run/systemd/system" ] || /usr/share/atop/atop.daily&
0 0 * * * root [ -d "/run/systemd/system" ] && systemctl restart atop
