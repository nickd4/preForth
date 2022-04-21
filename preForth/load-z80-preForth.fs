\ load z80 preForth on top of a host Forth system

include load-preForth.fs
include preForth-z80-rts.pre
include preForth-rts.pre
include preForth-z80-backend.pre
include preForth.pre

cold

bye
