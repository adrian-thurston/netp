" Vim syntax file
"
" Language: genf
" Author: Adrian Thurston

syntax clear

syntax keyword Type
	\ bool string long

syntax keyword Type
	\ message thread module attribute

syntax keyword Keyword
	\ starts sends to read write

syntax match optlit "-[\-A-Za-z0-9]*" contained

syntax region optionSpec
	\ matchgroup=kw_option start="\<option\>"
	\ matchgroup=plain end=";"
	\ contains=Type,optlit

hi link kw_option Keyword
hi link optlit String
 
let b:current_syntax = "genf"
