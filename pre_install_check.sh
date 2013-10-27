LAST_ARG="$_"
SCRIPT_FILE="$0"

# export OCI_HOME=<directory of Oracle instant client>
# export OCI_LIB_DIR=$OCI_HOME
# export OCI_INCLUDE_DIR=$OCI_HOME/sdk/include
# export OCI_VERSION=<the instant client major version number> 
ERR_MSG="errors:"
WARN_MSG="warnings:"

# script must be sourced so that exported variables are still in scope after execution
if [[ "$SCRIPT_FILE" == "${BASH_SOURCE[0]}" ]]; then
  echo "script must be sourced:
      source $0"
  exit 2
fi

# at least OCI_HOME must be set
if [[ -z "$OCI_HOME" ]]; then
  ERR_MSG="${ERR_MSG}
  - OCI_HOME not set"
fi

# exit if pre-conditions are not met
if [[ "$ERR_MSG" != "errors:" ]]; then
  echo "$ERR_MSG"
  return 1
fi

if [[ ! -d "$OCI_HOME" ]]; then
  ERR_MSG="$ERR_MSG
    - OCI_HOME ${OCI_HOME} is not a directory or does not exist"
fi

if [[ $ERR_MSG != "errors:" ]]; then
  echo "$ERR_MSG"
  return 1
fi

if [[ -z "$OCI_LIB_DIR" ]]; then
  WARN_MSG="${WARN_MSG}
  - OCI_LIB_DIR not set using OCI_HOME: $OCI_HOME"
  export OCI_LIB_DIR="$OCI_HOME"
fi

if [[ -z "$OCI_INCLUDE_DIR" ]]; then
  WARN_MSG="${WARN_MSG}
  - OCI_INCLUDE_DIR not set using $OCI_HOME/sdk/include: $OCI_HOME/sdk/include"
  export OCI_INCLUDE_DIR="$OCI_HOME/sdk/include"
fi

if [[ "$WARN_MSG" != "warnings:" ]]; then
  echo "$WARN_MSG"
fi

if [[ ! -d "$OCI_LIB_DIR" ]]; then
  ERR_MSG="$ERR_MSG
  - not a directory: OCI_LIB_DIR: $OCI_LIB_DIR"
fi


if [[ ! -e "$OCI_INCLUDE_DIR/occi.h" ]]; then
  ERR_MSG="$ERR_MSG
  - could not find: occi.h in OCI_INCLUDE_DIR: $OCI_INCLUDE_DIR"
fi

if [[ $ERR_MSG != "errors:" ]]; then
  echo "$ERR_MSG"
  return 1
fi

OS=`uname`

if [[ $OS == "Darwin" ]]; then
  DYNLIB_EXT="dylib"
else
  DYNLIB_EXT="so"
fi

# check libraries
check_lib() {
  LINK_NAME="$1"
  FILE_NAME="$2"

  if [[ ! -e "$FILE_NAME" ]]; then
    EXT_FILE_NAME=`ls $FILE_NAME*`
    if [[ "" != $EXT_FILE_NAME ]]; then
      FILE_NAME=$EXT_FILE_NAME
    else 
      ERR_MSG="$ERR_MSG\n  -could not find: ${FILE_NAME}*"
      return
    fi
  fi

  if [[ -h "$LINK_NAME" ]]; then
    TARGET=`readlink $LINK_NAME`
    if [[ $TARGET != $FILE_NAME ]]; then
        echo "  link $LINK_NAME does not have $FILE_NAME as target. Recreating!"
        rm $LINK_NAME
        ln -s "$FILE_NAME" "$LINK_NAME"
    fi
  else 
    echo "  link $LINK_NAME does not exist. Creating!"
    ln -s "$FILE_NAME" "$LINK_NAME"
  fi

}

if [[ -z $OCI_VERSION ]]; then
  export OCI_VERSION=11
fi

pushd $OCI_LIB_DIR > /dev/null
check_lib libnnz.${DYNLIB_EXT} libnnz${OCI_VERSION}.${DYNLIB_EXT}
check_lib libocci.${DYNLIB_EXT} libocci.${DYNLIB_EXT}.${OCI_VERSION}
check_lib libclntsh.${DYNLIB_EXT} libclntsh.${DYNLIB_EXT}.${OCI_VERSION}
popd > /dev/null

if [[ $ERR_MSG != "errors:" ]]; then
  echo "$ERR_MSG"
  return 1
fi

if [[ $OS == "Darwin" ]]; then
  if [[ -z "$DYLD_LIBRARY_PATH" ]]; then
    export DYLD_LIBRARY_PATH=$OCI_LIB_DIR
  else 
    export DYLD_LIBRARY_PATH=$OCI_LIB_DIR:$DYLD_LIBRARY_PATH
  fi
else
  ldconfig -p | grep "libaio.so" > /dev/null
  RESULT=$?
  if [[ $RESULT == 1 ]]; then
    if which apt-get > /dev/null ; then
      sudo apt-get install libaio
    elif which yum > /dev/null ; then
      sudo yum install libaio
    fi
  fi
  echo "$OCI_LIB_DIR" | sudo tee -a /etc/ld.so.conf.d/oracle_instant_client.conf
  sudo ldconfig
fi


echo "everythings looks dandy"
echo "  OCI_HOME: $OCI_HOME"
echo "  OCI_LIB_DIR: $OCI_LIB_DIR"
echo "  OCI_INCLUDE_DIR: $OCI_INCLUDE_DIR"
return 0

