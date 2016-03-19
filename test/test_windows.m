winlen = 128;
dftlen = 4096;
F = 2*pi*(0:dftlen-1)/dftlen;

% See gencoswin for coefficients

W = zeros(7, dftlen);
lgs = {};

win = rectwin(winlen); % Ok ...
W(1,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'rectwin';

win = hann(winlen); % Like in sigproc.h
W(2,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'hann';

win = hanning(winlen); % Seems to be the same as hann ...
W(3,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'hanning';

win = hamming(winlen); % Like in sigproc.h
W(4,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'hamming';

win = blackman(winlen); % Like in sigproc.h (alpha = 0.16)
W(5,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'blackman';

win = nuttallwin(winlen); % Wiki(sigproc.h): Blackman–Nuttall
W(6,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'Blackman–Nuttall';

win = blackmanharris(winlen); % Wiki(sigproc.h): Same
W(7,:) = mag2db(abs(fft(win./sum(win), dftlen)));
lgs{end+1} = 'blackmanharris';

% Flattop's Matlab is completely different to the one on Wikipedia

plot(F, W.');
legend(lgs);
xlim([0, 0.4]);
ylim([-110, 0]);
