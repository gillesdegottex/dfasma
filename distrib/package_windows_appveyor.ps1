$arch=$args[0]

#http://doc.qt.io/qt-5/windows-deployment.html
$VERSION = ((git describe --tags --always) | Out-String).Substring(1)
$VERSION = ${VERSION} -replace "`n|`r"
echo "Version: ${VERSION}"

If ($arch -eq 'x64') {
    echo "Packaging for 64bits Windows"
    $PACKAGENAME = "dfasma_${VERSION}_win64"
    $QTPATH = "\Qt\5.10\msvc2015_64"
}
Else {
echo "Packaging for 32bits Windows"
    $PACKAGENAME = "dfasma_${VERSION}_win32"
    $QTPATH = "\Qt\5.10\msvc2015"
}

echo "Packaging ${PACKAGENAME}"
echo " "

cd distrib

# Create a directory for the files to package
New-Item -ItemType directory -Name ${PACKAGENAME} | Out-Null

# Add the executable
Copy-Item c:\projects\$env:APPVEYOR_PROJECT_SLUG\release\dfasma.exe ${PACKAGENAME}

# Add libraries
Copy-Item c:\projects\$env:APPVEYOR_PROJECT_SLUG\lib\libfft\libfftw3-3.dll ${PACKAGENAME}
Copy-Item c:\projects\$env:APPVEYOR_PROJECT_SLUG\lib\libsndfile\bin\libsndfile-1.dll ${PACKAGENAME}
#Copy-Item c:\projects\$env:APPVEYOR_PROJECT_SLUG\external\sdif\easdif\bin\Easdif.dll ${PACKAGENAME} # Remove as long as Easdif doesn't compile on windows anymore

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
Else                 { $INNOSCRIPT = "DFasma_MSVC2012_Win32bit.iss" }
$env:Path += ";C:\\Program Files (x86)\\Inno Setup 5"
& "c:\Program Files (x86)\Inno Setup 5\ISCC.exe" /o. /dMyAppVersion=${VERSION} c:\projects\$env:APPVEYOR_PROJECT_SLUG\distrib\${INNOSCRIPT}

# Get out of distrib
cd ..
