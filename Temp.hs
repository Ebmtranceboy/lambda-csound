module Main where

import Csound.Base
import Hocico

main = dac $ mean [return $ loopWav 1 "riff.wav", midi $ onMsg $ \cps -> aux reverbsc 0.95 12000  $ hocico cps  (0.4*log(cps)/2) /2]