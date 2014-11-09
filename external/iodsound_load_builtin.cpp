/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "../src/ftsound.h"
#include "../external/wavfile/wavfile.h"

//#include <iostream>

QString FTSound::getAudioFileReadingDescription(){
    return QString("<p>Using a built-in minimal WAV file reader (supports only PCM 16 bit signed LE mono format)</p>");
}

void FTSound::load(int channelid){

    m_fileaudioformat = QAudioFormat(); // Clear the format

    // Create the file reader and read the format
    WavFile* pfile = new WavFile(this);
    if(!pfile->open(fileFullPath))
        throw QString("built-in WAV file reader: Cannot open the file.");

    m_fileaudioformat = pfile->fileFormat();

    // Check if the format is currently supported
    if(!m_fileaudioformat.isValid())
        throw QString("built-in WAV file reader: Format is invalid.");

    if(m_fileaudioformat.channelCount()>1)
        throw QString("built-in WAV file reader: This audio file has multiple audio channel, whereas the built-in reader can read files with only a single channel. Please convert this file into a mono audio file before re-opening it.");

    if(!((m_fileaudioformat.codec() == "audio/pcm") &&
         m_fileaudioformat.sampleType() == QAudioFormat::SignedInt &&
         m_fileaudioformat.sampleSize() == 16 &&
         m_fileaudioformat.byteOrder() == QAudioFormat::LittleEndian))
        throw QString("built-in WAV file reader: Supports only 16 bit signed LE mono format, whereas current format is "+formatToString(m_fileaudioformat));

    setSamplingRate(m_fileaudioformat.sampleRate());

    // Load the waveform data from the file
    pfile->seek(pfile->headerLength());

    QByteArray  buffer;
    qint64      toread = m_fileaudioformat.sampleSize()/8;
    qint64      red;
    buffer.resize(toread);
    while((red = pfile->read(buffer.data(),toread))) {
        if(red<toread)
            throw QString("built-in WAV file reader: The data are corrupted");

        // Decode the data for 16 bit signed LE mono format
        const qint16 value = *reinterpret_cast<const qint16*>(buffer.constData());
        wav.push_back(value/32767.0);
    }

    delete pfile;
}
