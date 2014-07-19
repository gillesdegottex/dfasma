                                DFasma
         A tool to compare mono audio files in time and frequency
                            version master
                https://github.com/gillesdegottex/dfasma


DFasma is an open-source software used to compare audio files (mono only) in
time and frequency. The comparison is first visual, using wavforms and spectra.
Listening segments of the loaded files is also possible in order to allow
perceptual comparison.

This software is coded in C/C++ using the Qt library (http://qt-project.org).
It is stored as a GitHub project (https://github.com/gillesdegottex/dfasma).


Goals
    * The interface and the audio files have to be loaded as quick as possible.
    * Any kind of lossless audio files should be easily loaded
        (it currently uses libsndfile (http://www.mega-nerd.com/libsndfile)
        supporting ~25 different file formats).
    * Manage mono audio files (stereo files could be addressed in the future)
    * All features should run on Linux, OS X and Migrosoft operating systems.
    * Even though there are basic functionnalities to align the signals in 
      amplitude, this software is basically NOT an audio file editor.


Copyright (c) 2014 Gilles Degottex <gilles.degottex@gmail.com>

License
    This software is under the GPL (v3) License. See the file LICENSE.txt
    or http://www.gnu.org/licenses/gpl.html
    All source files of any kind (code source, icons, any ressources), except
    the content of the 'external' directory, which are necessary or optional
    for compiling this software are under the same license.

Disclaimer
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
