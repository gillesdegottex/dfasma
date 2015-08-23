#http://doc.qt.io/qt-5/windows-deployment.html
VERSION=$(git describe --tags --always)
echo Version: $VERSION

PACKAGENAME=DFasma-$VERSION-Win64bit
QTPATH=/Qt/5.4/msvc2013_64_opengl

echo Packaging $PACKAGENAME
echo " "

mkdir $PACKAGENAME

# Add the executable
cp release/dfasma.exe $PACKAGENAME/

# Add libraries
cp lib/fftw-3.3.4-dll64/libfftw3-3.dll $PACKAGENAME/
cp lib/libsndfile-1.0.25-w64/bin/libsndfile-1.dll $PACKAGENAME/

# Add the Qt related libs
cd $PACKAGENAME/
#C:/Qt/5.2.1/msvc2012_64_opengl/bin/qtenv2.bat
export PATH=/C$QTPATH/bin:$PATH
echo $PATH
C:$QTPATH/bin/windeployqt.exe --no-translations dfasma.exe
cd ..

# Add the MSVC redistribution installer
cp C:/Qt/vcredist/vcredist_sp1_x64.exe $PACKAGENAME/

# Add the translations
# mkdir $PACKAGENAME/tr
# cp -r ../tr/fmit_*.ts $PACKAGENAME/tr/
# C:$QTPATH/bin/lrelease.exe $PACKAGENAME/tr/*
# rm -f $PACKAGENAME/tr/*.ts

#"C:/Program Files (x86)/Inno Setup 5/ISCC"

"C:/Program Files (x86)/Inno Setup 5/ISCC.exe" /o. /dMyAppVersion=$VERSION "DFasma_MSVC2012_OpenGL_Win64bit.iss"
