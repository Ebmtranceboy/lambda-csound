module Hocico(hocico, aux) where

import Csound.Dynamic(Rate(Ar,Kr), mopcs)
import Csound.Typed(Sig, pureTuple, unSig)
import Csound.Base -- (dac, onMsg, midi)


hocico :: Sig -> Sig -> (Sig, Sig)
hocico b1 b2 = pureTuple $ f <$> unSig b1 <*> unSig b2
    where f a1 a2 = mopcs "hocico" ([Ar,Ar],[Kr,Kr]) [a1,a2]

aux f a b p = f (fst p) (snd p) a b

--main = dac $  {-at (dumpWav "dump.wav") $-} midi $ onMsg $ \cps -> aux reverbsc 0.95 12000  $ hocico cps  (0.4*log(cps)/2) /2