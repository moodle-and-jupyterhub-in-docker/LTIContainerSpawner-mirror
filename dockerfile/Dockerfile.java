FROM jupyter/base-notebook

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
  && chmod a+rx /usr/bin/ipynb_* \
  && chmod a+r  /etc/skel/.vimrc /etc/skel/.bash* \
  && true

RUN  /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterhub -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda install --prefix /opt/conda jupyterlab-language-pack-ja-JP -y \
  && /opt/conda/bin/conda clean   --all -y \
  && true

RUN  apt-get update \
  && apt-get upgrade -y \
  && apt-get install -y --no-install-recommends \
     default-jre \
     default-jdk \
     apt-utils \
     binutils \
#     tini \
     make \
     g++ \
#     vim \
     git \
     language-pack-ja-base \
     language-pack-ja \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true

# install ijava
RUN git clone https://github.com/SpencerPark/IJava.git ijava \
  && (cd ijava; chmod a+rx gradlew; ./gradlew installKernel --prefix /opt/conda ) \
  && /opt/conda/bin/jupyter kernelspec list \
  && rm -rf ijava \
  && rm -rf /root/.gradle /root/.local \
  && true

# install plotly
RUN  /opt/conda/bin/conda install -c plotly plotly \
  && /opt/conda/bin/jupyter labextension install jupyterlab-plotly \
  && /opt/conda/bin/jupyter labextension install @jupyter-widgets/jupyterlab-manager plotlywidget \
  && /opt/conda/bin/conda clean   --all -y \
  && true

# install ijavascript
RUN  /opt/conda/bin/npm install -g ijavascript \
  && /opt/conda/bin/ijsinstall --install=global \
  && /opt/conda/bin/jupyter kernelspec list \
  && true

