$arch=$args[0]

#http://doc.qt.io/qt-5/windows-deployment.html
$VERSION = (git describe --tags --always) | Out-String
$VERSION = $VERSION -replace "`n|`r"
echo "Version: $VERSION"

If ($arch -eq 'x64') {
    echo "Packaging for 64bits Windows"
    $PACKAGENAME = "DFasma-$VERSION-Win64bit"
    $QTPATH = "\Qt\5.4\msvc2013_64_opengl"
}
Else {
echo "Packaging for 32bits Windows"
    $PACKAGENAME = "DFasma-$VERSION-Win32bit"
    $QTPATH = "\Qt\5.4\msvc2013_opengl"
}

echo "Packaging $PACKAGENAME"
echo " "

cd distrib

# Create a directory for the files to package
New-Item -ItemType directory -Name $PACKAGENAME | Out-Null

# Add the executable
Copy-Item c:\projects\dfasma\release\dfasma.exe $PACKAGENAME

# Add libraries
Copy-Item c:\projects\dfasma\lib\libfft\libfftw3-3.dll $PACKAGENAME
Copy-Item c:\projects\dfasma\lib\libsndfile\bin\libsndfile-1.dll $PACKAGENAME
Copy-Item c:\projects\dfasma\external\sdif\easdif\bin\Easdif.dll $PACKAGENAME

# Add the Qt related libs, qt translations and installer of MSVC redist.
cd $PACKAGENAME
#C:/Qt/5.4/msvc2013_opengl/bin/qtenv2.bat
#export PATH=\C$QTPATH\bin:$PATH
#echo $PATH
$env:Path += ";C:\$QTPATH\bin"
& c:$QTPATH\bin\windeployqt.exe --no-translations dfasma.exe
cd ..

# Run Inno setup to create the installer
If ($arch -eq 'x64') { $INNOSCRIPT = "DFasma_MSVC2012_Win64bit.iss" }
Else { $INNOSCRIPT = "DFasma_MSVC2012_Win32bit.iss" }

& "c:\Program Files (x86)\Inno Setup 5\ISCC.exe" /o. /dMyAppVersion=$VERSION c:\projects\dfasma\distrib\$INNOSCRIPT

# Get out of distrib
cd ..
