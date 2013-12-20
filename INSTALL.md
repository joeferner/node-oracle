# Detailed installation instructions

You need to download and install [Oracle instant client](http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html) from following links:

http://www.oracle.com/technetwork/database/features/instant-client/index-097480.html

1. Instant Client Package - Basic or Basic Lite: All files required to run OCI, OCCI, and JDBC-OCI applications
2. Instant Client Package - SDK: Additional header files and an example makefile for developing Oracle applications with Instant Client

For Windows, please make sure 12_1 version is used.

**Please make sure you download the correct packages for your system architecture, such as 64 bit vs 32 bit**
**Unzip the files 1 and 2 into the same directory, such as /opt/instantclient\_11\_2 or c:\instantclient\_12\_1**

On MacOS or Linux:

> There is an experimental script that checks the environment and in most cases only needs OCI_HOME
> to be set:
>
> `   source pre_install_check.sh`
> 
> it checks for the necessary environment variables, checks (and creates) the necessary links, 
> installs missing libraries etc.
> At this time the script is to be considered 'experimental' (use at your own risk).

1. Set up the following environment variables

MacOS/Linux:

```bash
export OCI_HOME=<directory of Oracle instant client>
export OCI_LIB_DIR=$OCI_HOME
export OCI_INCLUDE_DIR=$OCI_HOME/sdk/include
export OCI_VERSION=<the instant client major version number> # Optional. Default is 11.
export NLS_LANG=AMERICAN_AMERICA.UTF8
```
__If you do not set NLS_LANG, returned Chinese may be garbled.__

2. Create the following symbolic links

MacOS:

```
cd $OCI_LIB_DIR
ln -s libclntsh.dylib.11.1 libclntsh.dylib
ln -s libocci.dylib.11.1 libocci.dylib
```

Linux:

```
cd $OCI_LIB_DIR
ln -s libclntsh.so.11.1 libclntsh.so
ln -s libocci.so.11.1 libocci.so
```

`libaio` library is required on Linux systems:

* On Unbuntu/Debian

```
sudo apt-get install libaio1
```

* On Fedora/CentOS/RHEL

```
sudo yum install libaio
```

3. Configure the dynamic library path

MacOS:

```
export DYLD_LIBRARY_PATH=$OCI_LIB_DIR
```

Linux:

Add the shared object files to the ld cache:

```
# Replace /opt/instantclient_11_2/ with wherever you extracted the Basic Lite files to
echo '/opt/instantclient_11_2/' | sudo tee -a /etc/ld.so.conf.d/oracle_instant_client.conf
sudo ldconfig
```

On Windows, you need to set the environment variables:

If you have VisualStudio 2012 installed,

```bat
OCI_INCLUDE_DIR=C:\instantclient_12_1\sdk\include
OCI_LIB_DIR=C:\instantclient_12_1\sdk\lib\msvc\vc11
OCI_VERSION=<the instant client major version number> # Optional. Default is 11.
Path=...;c:\instantclient_12_1\vc11;c:\instantclient_12_1
```

**Please make sure c:\instantclient_12_1\vc11 comes before c:\instantclient_12_1**

If you have VisualStudio 2010 installed,

```bat
OCI_INCLUDE_DIR=C:\instantclient_12_1\sdk\include
OCI_LIB_DIR=C:\instantclient_12_1\sdk\lib\msvc\vc10
OCI_VERSION=<the instant client major version number> # Optional. Default is 11.
Path=...;c:\instantclient_12_1\vc10;c:\instantclient_12_1
```

**Please make sure c:\instantclient_12_1\vc10 comes before c:\instantclient_12_1**

