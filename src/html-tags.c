/*********************************************************************
html-tags.c: parse the html tags and attributes.
This was originally handled by the tidy library, but tidy is no longer maintained.
Bugs are accumulating, and we continue to work around those bugs,
in html-tidy.c and in decorate.c, and it's getting old.
So, at some point, this file will do the job,
and will be maintained by us.
I believe it is easier, in the long run, to roll our own, rather than using
yet another html library, but I could be wrong,
tidy may have left a bad taste in my mouth.
In any case, I don't think it's terribly hard,
certainly easier than a javascript parser, so let's give it a whirl.
Note that we can paste the tags directly into the edbrowse tree;
any other library, including tidy, builds its own tree of nodes,
which we then import into the edbrowse tree of tags.
*********************************************************************/

#include "eb.h"

