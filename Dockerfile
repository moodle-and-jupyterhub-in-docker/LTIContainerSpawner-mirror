#FROM jupyter/base-notebook
#FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
#FROM jupyter/tensorflow-notebook
#FROM jupyter/scipy-notebook
#FROM tensorflow/tensorflow:latest-gpu-jupyter
FROM tensorflow/tensorflow-gpu-conda
USER root
COPY bin/start.sh  /usr/local/bin
COPY bin/commit.sh /usr/local/bin
COPY bin/start-notebook.sh   /usr/local/bin
COPY bin/start-singleuser.sh /usr/local/bin
COPY bin/ipynb_conv   /usr/bin
COPY bin/ipynb_deploy /usr/bin
COPY bin/ipynb_submit /usr/bin
COPY etc/.bashrc /root
COPY etc/.bashrc /etc/skel
COPY etc/.bash_profile /root
COPY etc/.bash_profile /etc/skel
COPY etc/.vimrc /root
COPY etc/.vimrc /etc/skel
COPY etc/passwd /etc/passwd.orig
COPY etc/group  /etc/group.orig
RUN  chmod a+rx /usr/local/bin/* \
  && chmod a+rx /usr/bin/ipynb_*
#RUN  /opt/conda/bin/conda install --prefix /opt/conda conda==4.10.3 -y \
#  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterhub -y \
#  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterlab -y \
#  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
#  && /opt/conda/bin/conda clean   --all -y \
#  && true
#RUN  apt-get update \
#  && apt-get upgrade -y \
#  && apt-get install -y --no-install-recommends \
#     apt-utils \
#     tini \
#     g++ \
#     vim \
#     git \
#  && apt-get -y clean \
#  && rm -rf /var/lib/apt/lists/* \
#  && true
CMD ["start-notebook.sh"]
