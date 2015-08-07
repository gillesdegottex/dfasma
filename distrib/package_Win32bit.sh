#http://doc.qt.io/qt-5/windows-deployment.html
VERSION=$(git describe --tags --always)
echo Version: $VERSION

PACKAGENAME=DFasma-$VERSION-Win32bit
QTPATH=/Qt/5.2.1/msvc2012

echo Packaging $PACKAGENAME
echo " "

rm -fr DFasma-*-Win32bit
mkdir $PACKAGENAME

# Add the executable
cp ../../build-dfasma-Desktop_Qt_5_2_1_MSVC2012_32bit-Release/release/dfasma.exe $PACKAGENAME/

# Add libraries
cp ../../lib/fftw-3.3.4-dll32/libfftw3-3.dll $PACKAGENAME/
cp ../../lib/libsndfile-1.0.25-w32/bin/libsndfile-1.dll $PACKAGENAME/

# Add the Qt related libs
cd $PACKAGENAME/
#C:/Qt/5.2.1/msvc2012_64_opengl/bin/qtenv2.bat
export PATH=/C$QTPATH/bin:$PATH
echo $PATH
C:$QTPATH/bin/windeployqt.exe --no-translations dfasma.exe
cd ..

# Add the MSVC redistribution installer
cp C:/Qt/vcredist/vcredist_x86.exe $PACKAGENAME/

# Add the translations
# mkdir $PACKAGENAME/tr
# cp -r ../tr/fmit_*.ts $PACKAGENAME/tr/
# C:$QTPATH/bin/lrelease.exe $PACKAGENAME/tr/*
# rm -f $PACKAGENAME/tr/*.ts

#"C:/Program Files (x86)/Inno Setup 5/ISCC"

"C:/Program Files (x86)/Inno Setup 5/ISCC.exe" //o. //dMyAppVersion=$VERSION "DFasma_MSVC2012_Win32bit.iss"
