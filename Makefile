# vi: set tabstop=4 noautoindent:

#
all: install


install: 
	[ -d /var/lib/jupyterhub ] || mkdir /var/lib/jupyterhub
	[ -f /usr/local/etc/jupyterhub_docker_config.py ] || install -m 0644 etc/jupyterhub_docker_config.py /usr/local/etc
	[ -f /usr/local/etc/jupyterhub_podman_config.py ] || install -m 0644 etc/jupyterhub_podman_config.py /usr/local/etc
	[ -f /usr/local/etc/jupyterhub_ltictr_config.py ] || install -m 0644 etc/jupyterhub_ltictr_config.py /usr/local/etc
	[ -f /usr/local/etc/nbws.conf ]                   || install -m 0640 etc/nbws.conf  /usr/local/etc
	[ -f /usr/lib/systemd/system/jupyterhub.service ] || install -m 0644 etc/jupyterhub.service /usr/lib/systemd/system
	[ -f /usr/lib/systemd/system/feserver.service ]   || install -m 0644 etc/feserver.service /usr/lib/systemd/system
	install -m 0755 bin/fesvr /usr/local/bin
	install -m 0644 bin/feplg_nop.so  /usr/local/bin
	install -m 0644 bin/feplg_nbws.so /usr/local/bin
	[ -f /usr/local/etc/ltictr_proxy.conf ]             || install -m 0640 etc/ltictr_proxy.conf  /usr/local/etc
	[ -f /usr/lib/systemd/system/ltictr_proxy.service ] || install -m 0644 etc/ltictr_proxy.service  /usr/lib/systemd/system
	install -m 0755 bin/ltictr_proxy_server /usr/local/bin
	install -m 0755 bin/ltictr_api_server   /usr/local/bin
	systemctl daemon-reload


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
	rm -f /usr/local/bin/ltictr_proxy_server
	rm -f /usr/local/bin/ltictr_api_server
	rm -f /usr/local/etc/ltictr_proxy.conf
	rm -f /usr/lib/systemd/system/ltictr_proxy.service
	systemctl daemon-reload

