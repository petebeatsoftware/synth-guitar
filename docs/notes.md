Are the filters the most efficient and best choice?
Is spectral flux the best transient detection method for this application?
Do we need to recreate everything in processBlock()
If envelope false triggers turn out to be a problem, try a cheap time-domain check before a spectral veto. It would filter out swells and tremolo for well under 1 ms of extra delay.
Package name is better with spaces when not on Linux