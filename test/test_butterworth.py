import numpy as np
#import scipy.io.wavfile
#import scipy.signal
#import pysndfile

import matplotlib.pyplot as plt
plt.ion()

def db2mag(d):
    return 10.0**(d/20.0)
def mag2db(d):
    return 20.0*np.log10(d)

if  __name__ == "__main__" :
    print('Show Butterworth filter response')

    fs =16000
    fcut = 4000.0    
    os = [4, 8, 16, 32]
    
    omegap = 2*np.pi*fcut/float(fs)
    dftlen = 4096
    freqs = float(fs)*np.arange(dftlen/2+1)/float(dftlen)
    omegas = 2*np.pi*freqs/float(fs)
    
    for o in os:
        H = 1.0/np.sqrt(1 + (omegas/omegap)**(2*o))
        H = H**2 # Using filtfilt
        plt.plot(freqs, mag2db(H))
        #plt.plot(np.log10(freqs), mag2db(H))

    #plt.xlim([3, 4])
    plt.ylim([-100, 10])
    plt.xlabel('Frequency [Hz]')
    plt.ylabel('Amplitude [dB]')
    plt.grid()

    from IPython.core.debugger import  Pdb; Pdb().set_trace()
