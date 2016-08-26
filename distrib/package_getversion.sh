
DISTRIB=$(bash package_getdistrib.sh $1)

VERSION=$(git describe --tags --always |cut -c2-)
VERSION=$VERSION-git1$DISTRIB

echo $VERSION
