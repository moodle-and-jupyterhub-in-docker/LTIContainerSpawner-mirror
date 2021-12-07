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

RUN  /opt/conda/bin/conda install --prefix /opt/conda conda==4.11.0 -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterhub -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda -c conda-forge jupyterlab -y \
  && /opt/conda/bin/conda install --prefix /opt/conda jupyterlab-language-pack-ja-JP -y \
  && /opt/conda/bin/conda update  --prefix /opt/conda --all -y \
  && /opt/conda/bin/conda clean   --all -y \
  && true

RUN  apt-get update \
  && apt-get upgrade -y \
  && apt-get install -y --no-install-recommends \
     binutils \
     apt-utils \
     wget \
     php-cli php-dev php-pear \
     pkg-config \
     git \
     make \
     g++ \
     language-pack-ja-base \
     language-pack-ja \
  && apt-get -y clean \
  && rm -rf /var/lib/apt/lists/* \
  && true

# install zeromq and zmq php extension
RUN wget https://github.com/zeromq/zeromq4-1/releases/download/v4.1.5/zeromq-4.1.5.tar.gz \
  && tar -xvf zeromq-4.1.5.tar.gz \
  && cd zeromq-4.1.5 \
  && ./configure && make && make install \
  && cd .. \
  && rm -r zeromq-4.1.5 zeromq-4.1.5.tar.gz \
  && true

RUN git clone git://github.com/mkoppanen/php-zmq.git \
  && cd php-zmq \
  && phpize && ./configure \
  && make \
  && make install \
  && cd .. \
  && rm -r php-zmq \
  && echo "extension=zmq.so" > /etc/php/7.4/cli/conf.d/zmq.ini \
  && true

# install PHP composer
RUN cd .. \
  && wget https://getcomposer.org/installer -O composer-setup.php \
  && wget https://litipk.github.io/Jupyter-PHP-Installer/dist/jupyter-php-installer.phar \
  && php composer-setup.php \
  && php composer.phar self-update 1.9.3 \
  && php ./jupyter-php-installer.phar -vvv install \
  && mv composer.phar /usr/local/bin/composer \
  && rm -rf zeromq-* jupyter-php* \
  && rm composer-setup.php \
  && true

# install ijavascript
RUN  /opt/conda/bin/npm install -g ijavascript \
  && /opt/conda/bin/ijsinstall --install=global \
  && /opt/conda/bin/jupyter kernelspec list \
  && true

