# vi: set tabstop=4 noautoindent:

#
all: install


install: 
	[ -d /var/lib/jupyterhub ] || mkdir /var/lib/jupyterhub
	install -m 0755 etc/jupyterhub_docker_config.py /usr/local/etc
	install -m 0755 etc/jupyterhub_podman_config.py /usr/local/etc
	install -m 0755 etc/jupyterhub_ltids_config.py  /usr/local/etc
	install -m 0644 etc/jupyterhub.service /usr/lib/systemd/system
	install -m 0755 bin/fesvr /usr/local/bin
	install -m 0644 bin/feplg_nop.so  /usr/local/bin
	install -m 0644 bin/feplg_nbws.so /usr/local/bin
	install -m 0644 etc/feserver.service /usr/lib/systemd/system


clean:
	[ -d /var/lib/jupyterhub ] && rm /var/lib/jupyterhub/*


uninstall:
	systemctl stop feserver   || true
	systemctl stop jupyterhub || true
	[ -d /var/lib/jupyterhub ] && rm -f /var/lib/jupyterhub/*
	[ -f /usr/local/etc/jupyterhub_docker_config.py ]  && rm -f /usr/local/etc/jupyterhub_docker_config.py
	[ -f /usr/local/etc/jupyterhub_podman_config.py ]  && rm -f /usr/local/etc/jupyterhub_podman_config.py
	[ -f /usr/local/etc/jupyterhub_ltids_config.py  ]  && rm -f /usr/local/etc/jupyterhub_ltids_config.py
	[ -f /usr/lib/systemd/system/jupyterhub.service ]  && systemctl disable jupyterhub && rm -f /usr/lib/systemd/system/jupyterhub.service
	[ -x /usr/local/bin/fesvr ] && rm /usr/local/bin/fesvr
	[ -f /usr/local/bin/feplg_nop.so  ] && rm -f /usr/local/bin/feplg_nop.so
	[ -f /usr/local/bin/feplg_nbws.so ] && rm -f /usr/local/bin/feplg_nbws.so
	[ -f /usr/lib/systemd/system/feserver.service ]    && systemctl disable feserver   && rm -f /usr/lib/systemd/system/feserver.service
	systemctl daemon-reload

