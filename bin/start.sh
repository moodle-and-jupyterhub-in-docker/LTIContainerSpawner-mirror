#!/bin/bash
# Copyright (c) Jupyter Development Team.
# Distributed under the terms of the Modified BSD License.
#
# /usr/local/bin/start.sh   2021 09/28 v0.9.10
#       This is modified by Fumi.Iseki for LTIDockerSpawner 
#

PRG_NAME="start.sh"

set -e

# Exec the specified command or fall back on bash
if [ $# -eq 0 ]; then
    cmd=( "bash" )
else
    cmd=( "$@" )
fi

run-hooks () {
    # Source scripts or run executable files in a directory
    if [[ ! -d "$1" ]] ; then
        return
    fi
    echo "$PRG_NAME: running hooks in $1"
    for f in "$1/"*; do
        case "$f" in
            *.sh)
                echo "$PRG_NAME: running $f"
                source "$f"
                ;;
            *)
                if [[ -x "$f" ]] ; then
                    echo "$PRG_NAME: running $f"
                    "$f"
                else
                    echo "$PRG_NAME: ignoring $f"
                fi
                ;;
        esac
    done
    echo "$PRG_NAME: done running hooks in $1"
}

run-hooks /usr/local/bin/start-notebook.d

#
# Handle special flags if we're root
if [ $(id -u) == 0 ] ; then
    #
    # root mode
    #
    echo "$PRG_NAME: root mode."
    
    #
    # make home base directory
    HOME_DIR="/home"
    if [ "$NB_GROUP" != "" ]; then
        HOME_DIR="$HOME_DIR/$NB_GROUP"
        if [ ! -e "$HOME_DIR" ]; then
            mkdir $HOME_DIR
        fi
    fi
    echo "$PRG_NAME: setup home directory to $HOME_DIR"

    #
    # create new user account
    groupadd -f -g $NB_GID $NB_GROUP || true
    if [ ! $(id -u $NB_USER 2>/dev/null) ]; then
        useradd --home $HOME_DIR/$NB_USER -u $NB_UID -g $NB_GID -l $NB_USER -s /bin/bash || true
    fi
    #
    echo "$PRG_NAME: setup user to $NB_USER"
    echo "$PRG_NAME: /etc/passwd is ..."
    cat /etc/passwd | tail -3

    #
    # setup home directory
    echo "$PRG_NAME: start to relocate home dir"
    if [ "$NB_USER" != "jovyan" ]; then
        #
        if [ ! -e "$HOME_DIR/$NB_USER" ]; then
            mkdir -p $HOME_DIR/$NB_USER
        fi

        DROWN=`ls -ld $HOME_DIR/$NB_USER | grep ^d | awk -F" " '{print $3}'`
        if [ "$DROWN" != "$NB_USER" ]; then
            chown $NB_UID:$NB_GID $HOME_DIR/$NB_USER 
            chown $NB_UID:$NB_GID $HOME_DIR/$NB_USER/* || true
            chmod 0700 $HOME_DIR/$NB_USER
            chmod 0700 $HOME_DIR/$NB_USER/* || true 
        fi
        rm -rf /home/jovyan || true
        # 
        echo "$PRG_NAME: relocated home dir to $HOME_DIR/$NB_USER"
    fi 

    #
    # setup Jupyter work directory
    if [ ! -d $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR/$COURSE_DIR ]; then
        mkdir -p $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR/$COURSE_DIR || true
    fi
    chown $NB_UID:$NB_GID $HOME_DIR/$NB_USER/$PRJCT_DIR || true
    chown $NB_UID:$NB_GID $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR || true
    #
    if [ ! -z "$CHOWN_EXTRA" ]; then
        for extra_dir in $(echo $CHOWN_EXTRA | tr ',' ' '); do
            chown $CHOWN_EXTRA_OPTS $NB_UID:$NB_GID $extra_dir || true
        done
    fi
    echo "$PRG_NAME: setup Jupyter work directory to $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR ($NB_UID:$NB_GID)"

    # clean up and path to home directory
    cd $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR
    LKS=`ls -l | grep ^l | awk -F" " '{print $9}'`
    if [ "$LKS" != "" ]; then
        rm -f $LKS || true
    fi
    echo "$PRG_NAME: cleaned up symbolic link"

    #
    # setup new teacher group for docker volumes
    echo "$PRG_NAME: setup new teacher group for docker volumes"
    EGID=$NB_GID
    if [ "$NB_TEACHER" == "$NB_USER" ]; then
        if [ "$NB_THRGID" != "" ]; then
            groupadd -f -g $NB_THRGID $NB_THRGROUP || true
        else
            groupadd $NB_THRGROUP 
        fi
        usermod -aG $NB_THRGROUP $NB_USER || true
        
        EGID=`grep $NB_THRGROUP /etc/group | awk -F":" '{print $3}'`
        if [ "$EGID" == "" ]; then
            EGID=$NB_GID
        fi 
        #
        echo "$PRG_NAME: setup new teacher group ($EGID) for docker volumes"
        echo "$PRG_NAME: /etc/group is ..."
        cat /etc/group | tail -3
    fi

    #
    # setup docker volumes
    NB_VOLUMES=`echo $NB_VOLUMES | sed -e "s/[*;$\!\"\'&|\\<>?^%\(\)\{\}\n\r~]//g"`
    NB_SUBMITS=`echo $NB_SUBMITS | sed -e "s/[*;$\!\"\'&|\\<>?^%\(\)\{\}\n\r~]//g"`
    NB_PRSNALS=`echo $NB_PRSNALS | sed -e "s/[*;$\!\"\'&|\\<>?^%\(\)\{\}\n\r~]//g"`
    #
    echo "$PRG_NAME: task volumes are     : $NB_VOLUMES"
    echo "$PRG_NAME: submit volumes are   : $NB_SUBMITS"
    echo "$PRG_NAME: personal volumes are : $NB_PRDNALS"

    cd $HOME_DIR/$NB_USER/$PRJCT_DIR/$WORK_DIR

    echo "$PRG_NAME: start to link to task volumes"
    if [ "$NB_VOLUMES" != "" ]; then
        for VOLUME in $NB_VOLUMES; do
            FL=`echo $VOLUME | cut -d ':' -f 1`
            LK=`echo $VOLUME | cut -d ':' -f 2`
            #
            if [[ "$FL" != "" && "$LK" != ""  && -d "$FL" ]]; then
                FLOWN=`ls -ld $FL | awk -F" " '{print $3}'`
                if [[ "$FLOWN" == "root" && "$NB_TEACHER" == "$NB_USER" ]]; then
                    chown $NB_UID:$EGID $FL || true
                    chmod 2775 $FL || true
                fi
                #
                if [[ ! -e "$LK" || "$LK" == "." ]]; then
                    if [ "${LK:0:1}" != "-" ]; then
                        ln -s $FL $LK || true
                    else
                        if [ "$NB_TEACHER" == "$NB_USER" ]; then
                            ln -s $FL "${LK:1}" || true
                        fi
                    fi
                fi
            fi
        done
        echo "$PRG_NAME: linked to task volumes"
    fi

    #
    echo "$PRG_NAME: start to link to submit volumes"
    if [ "$NB_SUBMITS" != "" ]; then
        for SUBMIT in $NB_SUBMITS; do
            FL=`echo $SUBMIT | cut -d ':' -f 1`
            LK=`echo $SUBMIT | cut -d ':' -f 2`
            #
            if [[ "$FL" != "" && "$LK" != ""  && -d "$FL" ]]; then
                FLOWN=`ls -ld $FL | awk -F" " '{print $3}'`
                if [[ "$FLOWN" == "root" && "$NB_TEACHER" == "$NB_USER" ]]; then
                    chown $NB_UID:$EGID $FL || true
                    chmod 3777 $FL || true
                fi
                #
                if [[ ! -e "$LK" || "$LK" == "." ]]; then
                    if [ "${LK:0:1}" != "-" ]; then
                        ln -s $FL $LK || true
                    else
                        if [ "$NB_TEACHER" == "$NB_USER" ]; then
                            ln -s $FL "${LK:1}" || true
                        fi
                    fi
                fi
            fi
        done
        echo "$PRG_NAME: linked to submit volumes"
    fi

    #
    echo "$PRG_NAME: start to link to personal volumes"
    if [ "$NB_PRSNALS" != "" ]; then
        for PRSNAL in $NB_PRSNALS; do
            FL=`echo $PRSNAL | cut -d ':' -f 1`
            LK=`echo $PRSNAL | cut -d ':' -f 2`
            #
            if [[ "$FL" != "" && "$LK" != "" ]]; then
                if [ ! -d "$FL" ]; then
                    mkdir -p $FL || true
                    chown $NB_UID:$NB_GID $FL || true
                    chmod 0700 $FL || true
                fi
                #
                if [[ ! -e "$LK" || "$LK" == "." ]]; then
                    if [ "${LK:0:1}" != "-" ]; then
                        ln -s $FL $LK || true
                    else
                        if [ "$NB_TEACHER" == "$NB_USER" ]; then
                            ln -s $FL "${LK:1}" || true
                        fi
                    fi
                fi
                #
                VF=`echo $FL | sed -e "s/\/lms_prs_/\/lms_vol_/"`
                if [[ -d "$VF" && "$VF" != "$FL" ]]; then
                    cp --no-clobber -Rd $VF/* $FL || true
                    chown -R $NB_UID:$EGID $FL || true
                fi
            fi
        done
        echo "$PRG_NAME: linked to personal volumes"
    fi
   
    #
    # fix user information
    echo "$PRG_NAME: start to fix user information."
    if [[ "$NB_UID" != "$(id -u $NB_USER)" || "$NB_GID" != "$(id -g $NB_USER)" ]]; then
        if [ "$NB_GID" != "$(id -g $NB_USER)" ]; then
            groupadd -f -g $NB_GID -o ${NB_GROUP:-${NB_USER}}
        fi
        userdel $NB_USER || true
        useradd --home $HOME_DIR/$NB_USER -u $NB_UID -g $NB_GID -l $NB_USER -s /bin/bash 
        if [ "$NB_TEACHER" == "$NB_USER" ]; then
            usermod -aG $NB_THRGROUP $NB_USER
        fi
        # 
        echo "$PRG_NAME: fixed user information."
        echo "$PRG_NAME: /etc/passwd is ..."
        cat /etc/passwd | tail -3
    fi

    #
    # Enable sudo if requested
    echo "$PRG_NAME: start to grant sudo access"

    if [ -x /usr/bin/sudo ] ;then
        echo "$PRG_NAME: granting $NB_USER sudo access and appending $CONDA_DIR/bin to sudo path"
        if [ ! -d /etc/sudoers.d ]; then
            mkdir /etc/sudoers.d
        fi
        # Add $CONDA_DIR/bin to sudo secure_path
        sed -r "s#Defaults\s+secure_path\s*=\s*\"?([^\"]+)\"?#Defaults secure_path=\"\1:$CONDA_DIR/bin\"#" /etc/sudoers | \
                                                                                                       grep secure_path > /etc/sudoers.d/path 
        chmod 4111 /usr/bin/sudo 
        #
        if [[ "$GRANT_SUDO" == "1" || "$GRANT_SUDO" == 'yes' ]]; then
            echo "$NB_USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/notebook 
        fi
    else
        echo "$PRG_NAME: /usr/bin/sudo command is not available!!"
    fi
    
    #
    # notebook directory
    if [ "$NB_DIR" != "" ]; then
        cd $NB_DIR
    else
        cd $HOME_DIR/$NB_USER
    fi

    # Exec the command as NB_USER with the PATH and the rest of
    # the environment preserved
    run-hooks /usr/local/bin/before-notebook.d
    echo "$PRG_NAME: executing the command: ${cmd[@]}"
    exec sudo -E -H -u $NB_USER PATH=$PATH XDG_CACHE_HOME=$HOME_DIR/$NB_USER/.cache PYTHONPATH=${PYTHONPATH:-} "${cmd[@]}" 
    #
else
    #
    # rootless mode
    #
    echo "$PRG_NAME: rootless mode."

    #
    if [[ "$NB_UID" == "$(id -u jovyan 2>/dev/null)" && "$NB_GID" == "$(id -g jovyan 2>/dev/null)" ]]; then
        # User is not attempting to override user/group via environment
        # variables, but they could still have overridden the uid/gid that
        # container runs as. Check that the user has an entry in the passwd
        # file and if not add an entry.
        STATUS=0 && whoami &> /dev/null || STATUS=$? && true
        if [[ "$STATUS" != "0" ]]; then
            if [[ -w /etc/passwd ]]; then
                echo "Adding passwd file entry for $(id -u)"
                cat /etc/passwd | sed -e "s/^jovyan:/nayvoj:/" > /tmp/passwd
                echo "jovyan:x:$(id -u):$(id -g):,,,:/home/jovyan:/bin/bash" >> /tmp/passwd
                cat /tmp/passwd > /etc/passwd
                rm /tmp/passwd
            else
                echo 'Container must be run with group "root" to update passwd file'
            fi
        fi

        # Warn if the user isn't going to be able to write files to $HOME.
        if [[ ! -w /home/jovyan ]]; then
            echo 'Container must be run with group "users" to update files'
        fi
    else
        # Warn if looks like user want to override uid/gid but hasn't
        # run the container as root.
        if [[ ! -z "$NB_UID" && "$NB_UID" != "$(id -u)" ]]; then
            echo 'Container must be run as root to set $NB_UID'
        fi
        if [[ ! -z "$NB_GID" && "$NB_GID" != "$(id -g)" ]]; then
            echo 'Container must be run as root to set $NB_GID'
        fi
    fi

    # Warn if looks like user want to run in sudo mode but hasn't run
    # the container as root.
    if [[ "$GRANT_SUDO" == "1" || "$GRANT_SUDO" == 'yes' ]]; then
        echo 'Container must be run as root to grant sudo permissions'
    fi

    # Execute the command
    run-hooks /usr/local/bin/before-notebook.d
    echo "Executing the command: ${cmd[@]}"
    exec "${cmd[@]}"
fi
