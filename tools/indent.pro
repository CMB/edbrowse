// leave blanklines alone!
-nsob
-ncdb
-nbad
-nbap
-nbbb
// Measure levels of indenting - use 8 spaces = one tab (standard)
-ut
-ts8
-i4
-bli0
-cbi0
-nlp
// I really want -ci0, but that doesn't work!
// So I'm settling for ci3, which acts like -lp when an if statement
// is involved, and those are the ones that are usually long anyways.
-ci3
-ip0
-pi0
-cli0
-di2
// Braces are set by the coding standards of the linux kernel
-br
-brs
-cdw
-ce
-bs
// Breaking long lines.
-l80
-hnl
-psl
-nbbo
// Inserting spaces - usually I don't want it
-nprs
-npcs
-ncs
// can't prevent the space in switch (x)
-nsaf
-nsai
-nsaw
// the space in while(x--) ; is a good thing
-ss
