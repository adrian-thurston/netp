" Vim syntax file
"
" Language: genf
" Author: Adrian Thurston

syntax clear

syntax keyword Type
	\ bool string long

syntax keyword Type
	\ message packet thread module attribute kobj

syntax keyword Keyword
	\ starts sends to receives show store debug use

syntax match optlit "-[\-A-Za-z0-9]*" contained

syntax match comment "#[^\n]*\n"

syntax match dlit "\"[^\"]*\""

syntax region optionSpec
	\ matchgroup=kw_option start="\<option\>"
	\ matchgroup=plain end=";"
	\ contains=Type,optlit

hi link kw_option Keyword
hi link optlit String
hi link dlit String
 
let b:current_syntax = "genf"
