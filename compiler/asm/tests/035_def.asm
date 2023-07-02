org 0

db 11,22,33,44
defb 55,66,77,88
db 'string literal',0
defb "another string literal and bytes",0,0ah,0bh,0ch

dm 11,22,33,44
defm 55,66,77,88
dm 'string literal',0
defm "another string literal and bytes",0,0ah,0bh,0ch

dw 10000,20000,30000
defw -5000,-6000,40000

ds 50
ds 100,0
ds 200,55h

incbin 'tests/001_noarg.asm'
