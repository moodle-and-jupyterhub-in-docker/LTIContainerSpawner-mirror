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
  && chmod a+rx /usr/bin/ipynb_*

RUN  apt-get update \
#  && apt-get upgrade -y \
  && apt-get install -y --no-install-recommends \
     binutils \
     apt-utils \
     git \
     libfreetype6-dev\
     openssl \
     libssl-dev \
     clang \
     libpython3-dev \
     libblocksruntime-dev \
     language-pack-ja-base \
     language-pack-ja \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true

RUN  /opt/conda/bin/conda update  --prefix /opt/conda conda -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterhub -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda install --prefix /opt/conda jupyterlab-language-pack-ja-JP -y \
#  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
  && /opt/conda/bin/conda clean   --all -y \
  && true

#
ARG swift_url=https://download.swift.org/swift-5.5.1-release/ubuntu2004/swift-5.5.1-RELEASE/swift-5.5.1-RELEASE-ubuntu20.04.tar.gz

# Download and extract S4TF
WORKDIR /
ADD  $swift_url swift.tar.gz
RUN  tar -xzf swift.tar.gz --directory=usr --strip-components=2 \
  && rm swift.tar.gz \
  && git clone https://github.com/google/swift-jupyter.git \
  && true

# Install some python libraries that are useful to call from swift
WORKDIR /swift-jupyter
RUN  echo "Pillow" >> docker/requirements_py_graphics.txt \
  && python3 -m pip install --no-cache-dir -r docker/requirements.txt \
  && python3 -m pip install --no-cache-dir -r docker/requirements_py_graphics.txt \
#  && python3 register.py --sys-prefix --swift-toolchain  / > /dev/null \
  && python3 register.py --sys-prefix --swift-python-use-conda --use-conda-shared-libs --swift-toolchain  / > /dev/null \
  && chmod a+r * \
  && true

