import("oscillator.lib");
RANDMAX = 2147483647.0;
ratio(-3)=0.88997686;
ratio(-2)=0.93711560;
ratio(-1)=0.98047643;
ratio(0)=1;
ratio(1)=1.01991221;
ratio(2)=1.06216538;
ratio(3)=1.10745242;
ratio(i)=1.05^i;
freq = nentry("1-freq[in Hz]",60,0,3000,0.5);
detune = nentry("2-detune[normal]",0.5,0,1,0.01);
scale(x) = (x-1)*(detune^2+exp(20.0*(detune-1.0)))/2.0 + 1;
rand(0) = 0;
rand(i) = (rand(i-1)+12345)*1103515245;
phase_ini(i)=max(0,min(2047,int(rand(i)/RANDMAX*2048)));

process = par(i, 7, freq * scale(ratio(i-3)) : sawtooth : @(phase_ini(i))) :> _;


