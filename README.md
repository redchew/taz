# The Taz Programming Language
A re-implementation of the Rig/Ten programming language with a few minor
design changes and a cleaner codebase. Yes, I know my various recastings
of the language may seem a bit... unreliable.  But since the language is
still in its infancy at this point its the best time to re-think its
design and implementation to get rid of any discovered wrinkles before we
get saddled with backward compatibility expectations.

So far I've re-implemented the language a few times, to varying degrees
of completeness.  My first attempt was named Rig and saw some degree of
success, but was in essance a learning exercise since I'd no idea of how
to implement the language; it was doubltessly riddled with undiscovered
bugs, but worked to some degree on the surface.  The next attempt was
still called Rig, and it progressed somewhat further than the original,
but ultimately the runtime was encumbered by bad design and shoddy code.
My last attempt saw the language renamed to Ten, as I'd grown weary of
the connotations attached to the word 'rig,' and had no better ideas.
Finally, Taz will be my final attempt, if I can't achieve a satisfyig
result with this, after so many attempts; then I'd probably best leave
such things to better minds.

I hope, with this attempt, to simplify the language and interface to
some extent while implementating to a cleaner design and following
better testing and bookkeeping practices.