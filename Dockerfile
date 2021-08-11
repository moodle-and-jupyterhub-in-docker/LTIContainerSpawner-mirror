FROM jupyterhub/singleuser
#FROM jupyter/datascience-notebook
USER root
COPY start.sh /usr/local/bin
#COPY .bashrc .bash_profile .vimrc /etc/skel
#RUN  /opt/conda/bin/conda update -n base conda -y \
#  && /opt/conda/bin/conda update -c conda-forge jupyterlab -y
RUN  /opt/conda/bin/conda update -n base conda -y \
  && /opt/conda/bin/conda update -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda install jupyter -y \
  && /opt/conda/bin/conda install -c conda-forge nbgrader -y 
#  && /opt/conda/bin/jupyter nbextension install --sys-prefix --py nbgrader --overwrite \
#  && /opt/conda/bin/jupyter nbextension enable --sys-prefix --py nbgrader \
#  && /opt/conda/bin/jupyter serverextension enable --sys-prefix --py nbgrader
#RUN apt-get update \
# && apt-get upgrade -y \
# && apt-get install -y --no-install-recommends \
#    vim \
#    git \
# && apt-get -y clean \
# && rm -rf /var/lib/apt/lists/* \
# && /opt/conda/bin/conda update --all -y \
# && /opt/conda/bin/pip install git+https://github.com/NII-cloud-operation/Jupyter-LC_nblineage \
# && /opt/conda/bin/jupyter nblineage quick-setup \
# && true
