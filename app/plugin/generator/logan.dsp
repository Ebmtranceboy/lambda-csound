declare name "logan";
declare author "ebmtranceboy";
declare copyright "hellektropole";
declare version "1.00";
declare license "BSD"; 

import("math.lib");
import("basic.lib");

phasor(freq) = ((_, 1.) : fmod) ~ +(float(freq)/float(SR));

mirror(a, b, x) = a + mir(pos(x - a)) with {
	c = b - a;
	pos(y) = if(y < 0, y + 2*c*floor(1 - y/(2*c)), y);
	mir(y) = if(f2 < c, f2, 2*c - f2) with{
		f2 = fmod(y, 2*c);
	};};
       limit(x,a,b)=if(x<a,a,if(x>=b,b,x));


freq = nentry("freq", 60, 0, 3000, 0.5);
wet = nentry("wet", 0.03, 0, 1, 0.01);
master = 2*freq;

phm = phasor(master);
phm12 = phasor(freq);

env12 = 2.*phm12 - phm;
alpha = 2. - wet;

phmod = 0.5 - (phm - 0.5);
func = mirror(0., 1., alpha*phmod);
ramp = alpha*((2. - alpha)*phmod - func)/(alpha - 1.)/2.;
saw = -ramp*(1./alpha - 0.5);

func4 = limit((1. + 1./(1. - wet))*phm, 0., 1.);
pick = mirror(0., 1., 2.*func4);
buzine = pick*(0.5 - (1. - wet)/(2. - wet));

xone = phm + buzine;
u = mirror(-0.5, 0.5, 2.*xone - 0.5);
one = u*(4.*u^2 - 3.);

xtwo = phm + saw;
v = mirror(-0.5, 0.5, 2.*xtwo - 0.5);
two = v*(4.*v^2 - 3.);

process = ((1. - env12)*one + env12*two); // : moog_vcf(wet,1000);

<mdoc>
<equation>process</equation>
<notice />
<diagram>process</diagram>
</mdoc> 