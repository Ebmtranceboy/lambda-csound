<CsoundSynthesizer>

<CsOptions>

--output=dac --midi-device=a --nodisplays

</CsOptions>

<CsInstruments>

sr = 44100
ksmps = 64
nchnls = 2
0dbfs = 1.0
 massign 0, 19
gargg1 init 0.0
gargg0 init 0.0
giPort init 1
opcode FreePort, i, 0
xout giPort
giPort = giPort + 1
endop




instr 22

endin

instr 21
 event_i "i", 20, 604800.0, 1.0e-2
endin

instr 20
 turnoff2 19, 0.0, 0.0
 turnoff2 18, 0.0, 0.0
 exitnow 
endin

instr 19
ar0 = gargg0
ar1 = gargg1
ir5 active p1
ar2 upsamp k(ir5)
ar3 = sqrt(ar2)
ar2 = (1.0 / ar3)
if (ir5 < 2.0) then
    ar3 = 1.0
else
    ar3 = ar2
endif
ir9 ampmidi 1.0
ar2 upsamp k(ir9)
ir10 cpsmidi 
kr0 = log(ir10)
kr1 = (0.4 * kr0)
kr0 = (kr1 / 2.0)
ar4, ar5 hocico ir10, kr0
ar6 = (ar4 / 2.0)
ar4 = (ar5 / 2.0)
ir19 = 0.95
ir20 = 12000.0
ar5, ar7 reverbsc ar6, ar4, ir19, ir20
ar4 = (ar2 * ar5)
ar5 = (ar3 * ar4)
ar4 = (ar0 + ar5)
gargg0 = ar4
ar0 = (ar2 * ar7)
ar2 = (ar3 * ar0)
ar0 = (ar1 + ar2)
gargg1 = ar0
endin

instr 18
ar0 = gargg0
ar1 = gargg1
gargg0 = 0.0
gargg1 = 0.0
arl0 init 0.0
arl1 init 0.0
ir13 = 1.0
ar2 upsamp k(ir13)
ar3, ar4 diskin2 "riff.wav", ir13, 0.0, 1.0
ar5 = (ar3 + ar0)
ar0 = (ar5 / 2.0)
ir18 = 0.0
ir19 = 89.0
ir20 = 100.0
ar3 compress ar0, ar2, ir18, ir19, ir19, ir20, ir18, ir18, 0.0
ar0 = (ar3 * 0.8)
arl0 = ar0
ar0 = (ar4 + ar1)
ar1 = (ar0 / 2.0)
ar0 compress ar1, ar2, ir18, ir19, ir19, ir20, ir18, ir18, 0.0
ar1 = (ar0 * 0.8)
arl1 = ar1
ar0 = arl0
ar1 = arl1
 outs ar0, ar1
endin

</CsInstruments>

<CsScore>



f0 604800.0

i 22 0.0 -1.0 
i 21 0.0 -1.0 
i 18 0.0 -1.0 

</CsScore>

</CsoundSynthesizer>