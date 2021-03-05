#! /usr/bin/bash
#
# MkPackages
#
# Create Embedded Client deployable archives in packages directory
#
# Two packages are created, a compressed tar archive and a zip archive
#
# The script aborts if it is not run in an embedded-client directory and
# if the directory is not a clean git clone.
#
# The user's tmp directory is used as a workspace

pname=".MkPackages"
packages="packages"
scratch="${HOME}tmp/ec_archive_$$"
version=`git describe --tags --always | sed -e"s/-.*//" `
[ -d submodules/embedded-client ] && premium="-premium"
archive="embedded-client${premium}-${version}"

# log error and exit
# args message
err_exit() {
    echo "Error: $1"
    # rm -rf ${scratch}
    exit -1
}

# return true if the repo is either EC basic or EC premium
# args current directory
is_ec_dir() {
    if [ "$1" == "" ]; then
        echo "false"
    elif [ ! -d $1 ]; then
        echo "false"
    elif [ ! -d $1/libel -a ! -d $1/submodules/embedded-client ]; then
        echo "Cant find libel/ or submodules/embedded-client directories" > /dev/tty
        echo "false"
    elif [ ! -d plugins ]; then
        echo "Cant find plugins/ directory" > /dev/tty
        echo "false"
    elif [ ! -e $1/.git ]; then
        echo "Cant find .git/ directory" > /dev/tty
        echo "false"
    elif [ ! -e $1/README.md ]; then
        echo "Cant find README.md file" > /dev/tty
        echo "false"
    else
        echo "true"
    fi
}

# return true if the repo is clean
# args current directory
is_clean_repo() {
    # git status --porcelain is silent if all is clean
    git status --porcelain > /dev/tty
    git clean -ndx > /dev/tty
    if [ -z "$(git status --porcelain)" -a -z "$(git clean -ndx)" ]; then
        echo "true"
    else
        echo "false"
    fi
}

rm -rf ${packages}
if [[ $(is_ec_dir ${PWD}) == "true" ]]; then
   # This is an embedded client directory. Is it clean?

    if [[ $(is_clean_repo ${PWD}) == "true" ]]; then
       # It is clean so lets archive it

        if mkdir -p ${packages}; then
            if mkdir -p ${scratch}; then
                tar czf ${scratch}/${archive}.tgz --exclude=${packages} .
                [ -e ${scratch}/${archive}.tgz ] || err_exit "Failed to create Tar archive ${scratch}/${archive}.tgz"
                [ -e ${scratch}/${archive}.tgz ] && mv ${scratch}/${archive}.tgz ${packages}

                zip -qry ${scratch}/${archive}.zip . -x "${packages}/*"
                [ -e ${scratch}/${archive}.zip ] || err_exit "Failed to create Zip archive ${scratch}/${archive}.zip"
                [ -e ${scratch}/${archive}.zip ] && mv ${scratch}/${archive}.zip ${packages}
                rm -rf ${scratch}
            else
                echo "Error: can't make temp directory: ${scratch}"
            fi
        else
            echo "Error: can't make packages directory: ${packages}"
        fi
    else
        echo "Error: this directory is not a clean git repo"
    fi
else
    echo "Error: must execute $0 in an embedded-client directory"
fi
