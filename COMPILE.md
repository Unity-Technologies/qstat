# Compilation instructions for qStat

## Linux and other GNU systems

### Build
```shall
./configure && make
```
### Install
To install qstat run
```shell
make install
```
### Configuration
To see which configuration parameters can be tweaked, run
```shell
./configure --help
```

If you want to compile from GIT you need to first install `autoconf` and `automake`, then run
```shell
./autogen.sh
```

## Windows
Release build
```shell
nmake /f Makefile.noauto windows
```
Debug build
```shell
nmake /f Makefile.noauto windows_debug
```

## Solaris
```shell
make -f Makefile.noauto solaris
```

## HPUX
```shell
make -f Makefile.noauto hpux
```
