#
# modifiyed by Fumi.Iseki  2021/9/6
#

FROM jupyter/tensorflow-notebook

USER root
# install dependencies 
RUN apt-get update && apt-get upgrade -y && apt-get install -yq --no-install-recommends \
    apt-utils \
    make \
    g++ \
    php-cli php-dev php-pear \
    pkg-config 

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
RUN apt-get install -yq --no-install-recommends nodejs npm libzmq3-dev \
 && npm install -g ijavascript \
 && ijs --ijs-install=global \
 && true

#RUN cd .. && \
#    apt-get install -yq --no-install-recommends cmake && \
#    wget https://raw.githubusercontent.com/root-mirror/cling/master/tools/packaging/cpt.py && \
#    chmod +x cpt.py && \
#    ./cpt.py --check-requirements && ./cpt.py --create-dev-env Debug --with-workdir=./cling/

RUN cd .. \
 && wget https://root.cern.ch/download/cling/cling_2020-10-14_ROOT-ubuntu2004.tar.bz2 \
 && tar -xvjf cling_2020-10-14_ROOT-ubuntu2004.tar.bz2 \
 && rm cling_*.tar.bz2 \
 && mv cling_* cling \
 && true

RUN cd .. \
 && ln -s $(pwd)/cling/bin/* /usr/bin/ \
 && cd cling/share/cling/Jupyter/kernel \
 && pip install -e . \
 && jupyter-kernelspec install cling-cpp17 \
 && jupyter-kernelspec install cling-cpp14 \
 && jupyter-kernelspec install cling-cpp11 \
 && true

RUN chown -R $NB_USER /home/$NB_USER \
 && rm -rf /home/$NB_USER/.local/share/jupyter \
 && true

# install java jre for h2o
RUN apt-get install -yq openjdk-8-jre

#
RUN apt-get clean -y \
 && rm -rf /var/lib/apt/lists/* \
 && true

# Reset user from jupyter/base-notebook
USER $NB_USER

# install jupyter-bash
RUN /opt/conda/bin/pip install --no-cache-dir bash_kernel
RUN /opt/conda/bin/python -m bash_kernel.install 

# install h2o
RUN /opt/conda/bin/pip install --no-cache-dir --upgrade h2o \
 && /opt/conda/bin/pip install --no-cache-dir --upgrade pandas \
 && true

# disable authentication
RUN mkdir -p .jupyter
RUN echo "" >> ~/.jupyter/jupyter_notebook_config.py
RUN echo "c.NotebookApp.token = ''" >> ~/.jupyter/jupyter_notebook_config.py

