import numpy as np
#import scipy.io.wavfile
#import scipy.signal
import pysndfile

import matplotlib.pyplot as plt
plt.ion()

def db2mag(d):
    return 10.0**(d/20.0)

if  __name__ == "__main__" :
    print('Synthesise clicks and sinusoids at regular time and frequencies')
    
    fs = 16000

    syn = np.zeros(4*fs)
    ts = np.arange(len(syn))/float(fs)

    # Add some frequencies
    freqs = [0, fs/16.0, fs/2-fs/16.0, fs/2]
    amps = -32
    for freq in freqs:
        amp = db2mag(amps)
        print('Synthesise: {:8.2f}Hz at {}dB'.format(freq, amps))
        if freq==0.0 or freq==fs/2: amp/=2
        syn += amp*2.0*np.cos((2*np.pi*freq)*ts)

    # Add some clicks
    clicks = np.array([0.0, 1.0, 2.0, 3.0, (len(syn)-1)/float(fs)])
    syn[(clicks*fs).astype(np.int)] = 0.5

    #print(pysndfile.get_sndfile_encodings('wav'))
    pysndfile.sndio.write('synth_grid_fs'+str(fs)+'.wav', syn, rate=fs, format='wav', enc='float32')

    if 0:
        plt.plot(ts, syn, 'k')
        from IPython.core.debugger import  Pdb; Pdb().set_trace()
