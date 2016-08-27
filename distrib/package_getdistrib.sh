
if [[ -n "$1" ]]; then
    DISTRIB=$1
else
    DISTRIB=`lsb_release -d |sed 's/Description:\s//g' |sed 's/\s//g' |sed 's/\..*//g' | awk '{print tolower($0)}'`
fi
#if [ "${DISTRIB/Ubuntu}" != "$DISTRIB" ] ; then
#    DISTRIB=`echo $DISTRIB |sed 's/\(Ubuntu_[0-9]\+\.[0-9]\+\).*/\1/g'`
#fi

echo $DISTRIB
