BINFILE=$1

VERSION=$(bash package_getversion.sh $2)

BINARCH=`objdump -p $BINFILE |grep 'file format' |sed 's/^.*file format\s//g'`
if [ "${BINARCH/elf64}" = "${BINARCH}" ] ; then
    ARCH=i386
else
    ARCH=amd64
fi

echo dfasma_$VERSION\_$ARCH.deb
