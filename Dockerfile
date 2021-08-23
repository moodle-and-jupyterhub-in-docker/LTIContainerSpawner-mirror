#FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
FROM jupyter/tensorflow-notebook
USER root
COPY bin/start.sh /usr/local/bin
#COPY bin/start-notebook.sh    /usr/local/bin
#COPY bin/start-singleuser.sh  /usr/local/bin
COPY bin/ipynb_conv   /usr/bin
COPY bin/ipynb_deploy /usr/bin
RUN  chmod a+rx /usr/local/bin/*
RUN  chmod a+rx /usr/bin/ipynb_*
COPY etc/.bashrc /root
COPY etc/.bash_profile /root
COPY etc/.vimrc /root
COPY etc/passwd /etc/passwd.orig
COPY etc/group  /etc/group.orig
RUN  /opt/conda/bin/conda update  --prefix /opt/conda conda -y \
  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterhub -y \
  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
  && true
#RUN  apt-get update \
#  && apt-get upgrade -y \
#  && apt-get install -y --no-install-recommends \
#      g++ \
#     vim \
#     git \
#  && apt-get -y clean \
#  && rm -rf /var/lib/apt/lists/* \
#  && true
