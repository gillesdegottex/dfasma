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

#include <QAudioDecoder>

class AudioDecoder : public QObject
{
    Q_OBJECT

public:
    QAudioDecoder m_decoder;

    QString m_targetFilename;

    qreal m_progress;

//public:
    AudioDecoder();
    ~AudioDecoder() { }

    void setSourceFilename(const QString &fileName);
    void start();
    void stop();

    void setTargetFilename(const QString &fileName);

public slots:
    void readBuffer();
    void error(QAudioDecoder::Error error);
    void stateChanged(QAudioDecoder::State newState);
    void finished();

private slots:
    void updateProgress();

};
