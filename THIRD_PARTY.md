# Third-party code

## Mutable Instruments: Plaits, Rings, stmlib

Seyir's sound comes from Émilie Gillet's open-source Eurorack firmware:
Plaits (the melody's synthesis engines and the drone), Rings (the resonator
voices and the SPACE reverb) and stmlib, which both use. The code is in
`ThirdParty/mutable`, copied from https://github.com/pichenettes/eurorack
(see `ThirdParty/mutable/UPSTREAM` for the exact commits).

One change was made: `plaits/user_data.h` includes `<cstdio>` so it builds on
a desktop. Seyir runs the code at 48 kHz, as on the modules, and resamples to
the host's rate.

Seyir isn't affiliated with or endorsed by Mutable Instruments.

```
Except when noted otherwise, all code is copyright Emilie Gillet and is
released under the MIT License with the following notice:

Copyright 2012 Emilie Gillet.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

Each source file carries its own copyright line and the same MIT notice.
