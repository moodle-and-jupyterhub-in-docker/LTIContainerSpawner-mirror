#FROM jupyter/base-notebook
FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
#FROM jupyter/tensorflow-notebook
#FROM jupyter/scipy-notebook
USER root
ADD  bin/start.sh \
     bin/commit.sh \
     bin/start-notebook.sh \
     bin/start-singleuser.sh \
     /usr/local/bin/
ADD  bin/ipynb_conv \
     bin/ipynb_conv.php \
     bin/ipynb_deploy \
     bin/ipynb_deploy_php \
     bin/ipynb_setup \
     bin/ipynb_submit \
     /usr/bin/
ADD  etc/.bashrc \
     etc/.bash_profile \
     etc/.vimrc \
     /root/
ADD  etc/.bash_profile \
     etc/.bashrc \
     etc/.vimrc \
     /etc/skel/
ADD  etc/passwd.orig \
     etc/group.orig \
     /etc/
RUN  chmod a+rx /usr/local/bin/* \
  && chmod a+rx /usr/bin/ipynb_*
RUN  /opt/conda/bin/conda install --prefix /opt/conda conda==4.10.3 -y \
  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterhub==1.4.2 -y \
  && /opt/conda/bin/conda install --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
  && /opt/conda/bin/conda clean   --all -y \
  && true
RUN  /opt/conda/bin/conda install --prefix /opt/conda jupyterhub-singleuser -y
RUN  apt-get update \
  && apt-get upgrade -y \
#  && apt-get install -y --no-install-recommends \
#     apt-utils \
#     sudo \
#     tini \
#     g++ \
#     vim \
#     git \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true
#CMD ["start-notebook.sh"]
