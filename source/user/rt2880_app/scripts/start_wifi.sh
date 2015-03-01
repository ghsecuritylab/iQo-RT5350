#!/usr/bin

ifconfig ra0 up
iwpriv ra0 set NetworkType=Infra
iwpriv ra0 set AuthMode=WPAPSK
iwpriv ra0 set EncrypType=AES
iwpriv ra0 set WPAPSK="qodome2014"
iwpriv ra0 set SSID="qodome"

udhcpc -p /var/run/udhcpc.pid -s /sbin/udhcpc.sh -i ra0 &

