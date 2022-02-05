# vi: set tabstop=4 noautoindent:

#
all: install


install: 
	[ -d /var/lib/jupyterhub ] || mkdir /var/lib/jupyterhub
	install -m 0644 etc/jupyterhub_docker_config.py /usr/local/etc
	install -m 0644 etc/jupyterhub_podman_config.py /usr/local/etc
	install -m 0644 etc/jupyterhub_ltictr_config.py /usr/local/etc
	install -m 0644 etc/jupyterhub.service /usr/lib/systemd/system
	install -m 0755 bin/fesvr /usr/local/bin
	install -m 0644 bin/feplg_nop.so  /usr/local/bin
	install -m 0644 bin/feplg_nbws.so /usr/local/bin
	install -m 0640 etc/nbws.conf  /usr/local/etc
	install -m 0644 etc/feserver.service /usr/lib/systemd/system


clean:
	rm -f /var/lib/jupyterhub/*


uninstall:
	systemctl stop jupyterhub    || true
	systemctl stop feserver      || true
	systemctl disable jupyterhub || true
	systemctl disable feserver   || true
	rm -f /var/lib/jupyterhub/*
	rm -f /usr/local/etc/jupyterhub_docker_config.py
	rm -f /usr/local/etc/jupyterhub_podman_config.py
	rm -f /usr/local/etc/jupyterhub_ltictr_config.py
	rm -f /usr/lib/systemd/system/jupyterhub.service
	rm -f /usr/local/bin/fesvr
	rm -f /usr/local/bin/feplg_nop.so
	rm -f /usr/local/bin/feplg_nbws.so
	rm -f /usr/local/etc/nbws.conf
	rm -f /usr/lib/systemd/system/feserver.service
	systemctl daemon-reload

