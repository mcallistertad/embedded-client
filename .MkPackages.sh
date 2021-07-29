#! /usr/bin/bash
#
# MkPackages <path/README.pdf>
#
# Create Embedded Client deployable archives in packages directory
#
# Two packages are created, a compressed tar archive and a zip archive
#
# The script aborts if it is not run in an embedded-client directory and
# if the directory is not a clean git clone. Also if README.pdf is missing
#
# Create pdf by running the following commmand in the directory where README.md resides
# % grip . 8080
#
# Open browser on localhost:8080 and print the pages to pdf file README.pdf
#
# The user's tmp directory is used as a workspace

if [ '$1' == '--help' ]; then
    echo 'usage: $0 [--force] <README.pdf>'
    echo ''
    echo 'force      continue even if SW_VERSION conflicts with previous tags'
    echo 'README.pdf must reference a pdf file which should be the current'
    echo '           formatted version of README.md'
    exit 0
fi

force=false
if [ "$1" == "-f" -o "$1" == "--force" ]; then
    force=true
    shift
fi

pname=".MkPackages"
branch=`git status | grep "^On branch " | sed -e"s/On branch //"`
packages="packages"
scratch=$(mktemp -d ) || { echo "Failed to create temp file"; exit 1; }
version=`git describe --tags --always | sed -e"s/-.*//" `
[ -d submodules/embedded-client ] && premium="-premium"
archive="embedded-client${premium}-${version}"

[ -e "$1" -a "$(file --mime-type "$1" | sed -e"s/.*: //")" == "application/pdf" ] || { echo "${pname} expects README.pdf as argument"; exit 1; }
pdf="$1"

if [ "${premium}" != "" ]; then
    libel_dir="submodules/embedded-client/libel/"
else
    libel_dir="libel/"
fi

echo "On branch ${branch}"

# log error and exit
# args message and force to continue
err_exit() {
    echo "Error: $1"
    rm -rf ${scratch}
    [ "$2" != "true" ] && exit -1
}

# error if SW_VERSION conflicts with that used in previous release
# A conflict is considered to be a version numerically less or equal
check_sw_versions() {
    sw_versions=
    version_bad=

    cd ${libel_dir}
    versions=$( git tag -l --sort=-version:refname )
    sw_version=$( grep "define.*SW_VERSION" libel.c | sed -e"s/.* //" )
    current_ver=$(git describe --dirty --always --tags)

    echo "Software version info - ${sw_version}"
    echo "======== ======= ========="
    for v in ${versions}; do
        sw_v="$( git show ${v}:libel/libel.c | grep "define.*SW_VERSION" | sed -e"s/.* //" )"
        [ "$(( $(( ${sw_version} )) <= $(( ${sw_v} )) ))" == "1" -a "${current_ver}" != "${v}" ] && version_bad="${v}: ${sw_v}"
        [ "${sw_v}" == "" ] || echo "SW_VERSION: ${sw_v} release ${v}"
    done
    [ "${version_bad}" != "" ] && err_exit "SW_VERSION in libel/libel.c conflicts with release ${version_bad}" ${force}
    cd - >/dev/null
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
    pushd $1 > /dev/null
    # remove any tmp or garbage files
    git clean -dxf > /dev/null || err_exit "Failed to clean tmp copy: $1"

    # git status --porcelain is silent if all is clean
    echo "git status:" `pwd` > /dev/tty
    git status --porcelain > /dev/tty || err_exit "git failed to get status for : $1"
    echo "git clean:"   `pwd`> /dev/tty
    git clean -ndx > /dev/tty || err_exit "git failed to clean: $1"
    if [ -z "$(git status --porcelain)" -a -z "$(git clean -ndx)" ]; then
        echo "true"
    else
        echo "false"
    fi
    popd > /dev/null
}

check_sw_versions

# check if this is a shallow repo
if [ $(git rev-parse --is-shallow-repository) == "false" ]; then
    echo "Error: git reports this is not a shallow repo"
    err_exit "Use: git clone --depth 1 --shallow-submodules --branch master --recursive git@github.com:..."
fi

rm -rf ${packages}

if [[ $(is_ec_dir ${PWD}) == "true" ]]; then
    if [[ $OSTYPE == 'darwin'* ]]; then
        echo 'Running on macOS'
        if command -v gcp &> /dev/null ; then
            CP=cp
        else
            err_exit 'gcp not found. Maybe do "brew install coreutils"?'
        fi
    else
        CP=cp
    fi

    $CP -r . ${scratch}/${archive} || err_exit "Failed to copy package files to ${scratch}"

    # Is the directory a clean copy of the client files?
    if [[ $(is_clean_repo ${scratch}/${archive}) == "true" ]]; then
       # It is clean so lets archive it

        if mkdir -p ${packages}; then
            $CP ${pdf} ${scratch}/${archive}/README.pdf
            cd ${scratch}
            tar czf ${scratch}/${archive}.tgz --exclude=${packages} ${archive}
            [ -e ${scratch}/${archive}.tgz ] || err_exit "Failed to create Tar archive ${scratch}/${archive}.tgz"

            zip -qry ${archive}.zip ${archive} -x "${packages}/*"
            [ -e ${scratch}/${archive}.zip ] || err_exit "Failed to create Zip archive ${scratch}/${archive}.zip"
            cd - >/dev/null

            [ -e ${scratch}/${archive}.tgz ] && mv ${scratch}/${archive}.tgz ${packages}
            [ -e ${scratch}/${archive}.zip ] && mv ${scratch}/${archive}.zip ${packages}
            rm -rf ${scratch}
            if [ -e ${packages}/${archive}.tgz -a -e ${packages}/${archive}.zip ]; then
                echo ${PWD}/${packages}:
                ls -l ${PWD}/${packages}
                echo "Tar Archive packages successfully created." $( tar tzf ${packages}/${archive}.tgz | wc -l ) "files"
                echo "Zip Archive packages successfully created." $( unzip -l ${packages}/${archive}.zip | tail -1 | sed -e"s/.*  //" )
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
