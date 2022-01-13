# LTIContainerSpawner

## A Study of Moodle(LMS) - JupyterHub Integration with LTI Custom Parameters.

### Install
```
# git clone https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner.git
# cd lticontainerspawner
# make install
```
##### for JupyterHub 
``` 
# vi /usr/local/etc/jupyter_XXX_config.py
# vi /usr/lib/systemd/system/jupyterhub.service
# systemctl enable jupyterhub.service
# systemctl start  jupyterhub.service
```
##### for Feserver
```
# vi /usr/local/etc/nbws.conf
# vi /usr/lib/systemd/system/feserver.service
# systemctl enable feserver.service
# systemctl start  feserver.service
```

### Wiki
- English:  https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner/-/wikis/Moodle---JupyterHub
- Japanese: https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner/-/wikis/Moodle---JupyterHub-(J)

### Please see also bellow Wiki 
- English:  https://gitlab.nsl.tuis.ac.jp/iseki/mod_lticontainer/-/wikis/mod_lticontainer
- Japanese: https://gitlab.nsl.tuis.ac.jp/iseki/mod_lticontainer/-/wikis/mod_lticontainer-(J)
