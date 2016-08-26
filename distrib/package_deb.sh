BINFILE=$1

DISTRIB=$(bash package_getdistrib.sh $2)
VERSION=$(bash package_getversion.sh $2)

echo "Packaging DFasma "$VERSION
echo "Distribution: "$DISTRIB
BINARCH=`objdump -p $BINFILE |grep 'file format' |sed 's/^.*file format\s//g'`
if [ "${BINARCH/elf64}" = "${BINARCH}" ] ; then
    ARCH=i386
else
    ARCH=amd64
fi
echo "Architecture: "$ARCH

PKGNAME=dfasma_$VERSION\_$ARCH
echo "Package name "$PKGNAME

rm -fr $PKGNAME
mkdir -p $PKGNAME

echo ' '
echo "List dependencies:"
DEPS=`objdump -p $BINFILE |grep NEEDED |awk '{ print $2 }'`
for dep in $DEPS; do
    dpkg -S $dep |grep -v chefdk
    deplist=`dpkg -S $dep |grep -v chefdk |sed 's/:.*$//g'`
    depdpkg=`echo "$deplist" |sort |uniq`
    depcurver=`dpkg -s $depdpkg |grep 'Version' |awk '{ print $2 }' |sed 's/:.*$//g' |sed 's/-.*$//g' |sed 's/+.*$//g'`
    echo "Dependency "$dep"    in package:"$depdpkg"    version:"$depcurver
    echo $depdpkg >> Depends_$PKGNAME
done
DEPENDS=`cat Depends_$PKGNAME |sort |uniq`
rm -f Depends_$PKGNAME
DEPENDS=`echo $DEPENDS |sed 's/ /,\ /g'`
echo ' '
echo "Automatic detection of Depends: "$DEPENDS
# This list if too dependent on external repo used for Travis CI ...
# ... so let's overwrite it with a predefined list of packages
# (http://packages.ubuntu.com/precise/allpackages can help to build it)
DEPENDS=`cat package_deb.Depends_$DISTRIB`
# If cannot list the dependencies properly, skip them ...
# DEPENDS=""
echo "Replaced by predefined list Depends: "$DEPENDS
echo ' '


# Build the file tree

# Copy files
mkdir -p $PKGNAME/DEBIAN
cp package_deb.control $PKGNAME/DEBIAN/control
sed -i "s/^Version:.*$/Version: $VERSION/g" $PKGNAME/DEBIAN/control
sed -i "s/^Architecture:.*$/Architecture: $ARCH/g" $PKGNAME/DEBIAN/control
sed -i "s/^Depends:.*$/Depends: $DEPENDS/g" $PKGNAME/DEBIAN/control

# The binary
mkdir -p $PKGNAME/usr/bin
cp $BINFILE $PKGNAME/usr/bin/

# Any legal and info txt
mkdir -p $PKGNAME/usr/share/doc/dfasma
cp ../README.txt $PKGNAME/usr/share/doc/dfasma/
cp ../LICENSE.txt $PKGNAME/usr/share/doc/dfasma/

# The menu related files
mkdir -p $PKGNAME/usr/share/appdata
cp dfasma.appdata.xml $PKGNAME/usr/share/appdata/
mkdir -p $PKGNAME/usr/share/applications
cp dfasma.desktop $PKGNAME/usr/share/applications/
mkdir -p $PKGNAME/usr/share/menu
cp dfasma.menu $PKGNAME/usr/share/menu/dfasma

# The icon
mkdir -p $PKGNAME/usr/share/icons/hicolor/scalable/apps
cp ../icons/dfasma.svg $PKGNAME/usr/share/icons/hicolor/scalable/apps/
mkdir -p $PKGNAME/usr/share/icons/hicolor/128x128/apps
cp ../icons/dfasma.png $PKGNAME/usr/share/icons/hicolor/128x128/apps/

INSTALLEDSIZE=`du $PKGNAME/usr/ |tail -n 1 |awk '{print $1}'`
sed -i "s/^Installed-Size:.*$/Installed-Size: $INSTALLEDSIZE/g" $PKGNAME/DEBIAN/control

dpkg-deb --build $PKGNAME

ls $PKGNAME.deb
