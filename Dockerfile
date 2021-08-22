#FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
FROM jupyter/tensorflow-notebook
USER root
COPY bin/start.sh /usr/local/bin
COPY bin/ipynb_conv   /usr/bin
COPY bin/ipynb_deploy /usr/bin
RUN  chmod a+rx /usr/bin/ipynb_*
COPY ./etc/.bashrc /root
COPY ./etc/.bash_profile /root
COPY ./etc/.vimrc /root
COPY ./etc/passwd /etc/passwd.orig
COPY ./etc/group  /etc/group.orig
RUN  /opt/conda/bin/conda update -n base conda -y \
  && /opt/conda/bin/conda update --prefix /opt/conda --all -y \
  && /opt/conda/bin/conda update -c conda-forge jupyterlab -y \
  && true
#RUN  apt-get update \
#  && apt-get upgrade -y \
#  && apt-get install -y --no-install-recommends \
#     vim \
#     git \
#  && apt-get -y clean \
#  && rm -rf /var/lib/apt/lists/* \
#  && /opt/conda/bin/conda update --all -y \
#  && /opt/conda/bin/conda update --prefix /opt/conda --all -y \
#  && true
