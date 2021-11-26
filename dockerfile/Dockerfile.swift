# Start from S4TF base image
#FROM gcr.io/swift-tensorflow/base-deps-cuda10.2-cudnn7-ubuntu18.04
#FROM gcr.io/swift-tensorflow/base-deps-cuda11.0-cudnn8-ubuntu18.04
FROM gcr.io/swift-tensorflow/base-deps-ubuntu20.04
#FROM jupyter/tensorflow-notebook

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

RUN  pip3 install --upgrade pip \
  && pip3 install jupyterhub \
  && pip3 install jupyterlab \
  && true

RUN  apt-get update \
#  && apt-get upgrade -y \
  && apt-get install -y --no-install-recommends \
     sudo \
     git \
     libfreetype6-dev\
     openssl \
     libssl-dev \
     clang \
     libpython3-dev \
     libblocksruntime-dev \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true

#
# Allow the caller to specify the toolchain to use
#ARG swift_tf_url=https://storage.googleapis.com/swift-tensorflow-artifacts/nightlies/latest/swift-tensorflow-DEVELOPMENT-cuda10.2-cudnn7-ubuntu18.04.tar.gz
#ARG swift_tf_url=https://storage.googleapis.com/swift-tensorflow-artifacts/nightlies/latest/swift-tensorflow-DEVELOPMENT-stock-ubuntu20.04.tar.gz
#ARG swift_tf_url=https://storage.googleapis.com/swift-tensorflow-artifacts/releases/v0.13/swift-tensorflow-RELEASE-0.13-ubuntu18.04.tar.gz
ARG swift_tf_url=https://storage.googleapis.com/swift-tensorflow-artifacts/releases/v0.13/swift-tensorflow-RELEASE-0.13-ubuntu20.04.tar.gz

# Download and extract S4TF
WORKDIR /
ADD  $swift_tf_url swift.tar.gz
RUN  tar -xzf swift.tar.gz --directory=usr --strip-components=1 \
  && rm swift.tar.gz \
  && git clone https://github.com/google/swift-jupyter.git \
  && true

# Install some python libraries that are useful to call from swift
WORKDIR /swift-jupyter
RUN  echo "Pillow" >> docker/requirements_py_graphics.txt \
  && python3 -m pip install --no-cache-dir -r docker/requirements.txt \
  && python3 -m pip install --no-cache-dir -r docker/requirements_py_graphics.txt \
  && python3 register.py --sys-prefix --swift-toolchain  / > /dev/null \
  && chmod a+r * \
  && true

ENV PATH="$PATH:/usr/local/bin:/usr/bin"
CMD ["start-notebook.sh"]

