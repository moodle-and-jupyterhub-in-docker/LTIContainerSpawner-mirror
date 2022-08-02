FROM jupyter/base-notebook
#FROM jupyter/datascience-notebook
#FROM jupyter/tensorflow-notebook
#FROM jupyter/scipy-notebook

USER root

ADD  bin/start.sh \
     bin/start-notebook.sh \
     bin/start-singleuser.sh \
     bin/commit.sh \
     bin/ipynb_conv \
     bin/ipynb_conv.php \
     bin/ipynb_deploy \
     bin/ipynb_deploy_php \
     bin/ipynb_setup \
     bin/ipynb_submit \
     bin/submit \
     bin/ipynb_extract \
     bin/extract \
     bin/ipynb_tocsv \
     bin/tocsv \
     /usr/local/bin/
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
  && chmod a+r  /etc/skel/.vimrc /etc/skel/.bash* \
  && true

RUN  /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterhub -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda install --prefix /opt/conda jupyterhub-singleuser -y \
  && /opt/conda/bin/conda install --prefix /opt/conda jupyterlab-language-pack-ja-JP -y \
#  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
  && /opt/conda/bin/conda clean   --all -y \
  && true

RUN  apt-get update \
#  && apt-get upgrade -y \
  && apt-get install -y --no-install-recommends \
#     binutils \
#     apt-utils \
#     sudo \
#     tini \
#     g++ \
#     vim \
#     git \
     language-pack-ja-base \
     language-pack-ja \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true

#CMD ["start-notebook.sh"]
