FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
USER root
COPY bin/start.sh /usr/local/bin
COPY bin/ipynb_conv /usr/bin
RUN  chmod a+rx /usr/bin/ipynb_conv
#COPY ./etc/.bashrc ./etc/.bash_profile ./etc/.vimrc /etc/skel
RUN  /opt/conda/bin/conda update -n base conda -y \
  && /opt/conda/bin/conda update --prefix /opt/conda --all -y \
#  && /opt/conda/bin/conda update -c conda-forge jupyterlab -y
  && true
#RUN  /opt/conda/bin/conda update -n base conda -y \
#  && /opt/conda/bin/conda update -c conda-forge jupyterlab -y \
#  && /opt/conda/bin/conda install jupyter -y \
#  && true
#RUN apt-get update \
# && apt-get upgrade -y \
# && apt-get install -y --no-install-recommends \
#    vim \
#    git \
# && apt-get -y clean \
# && rm -rf /var/lib/apt/lists/* \
# && /opt/conda/bin/conda update --all -y \
# && true
