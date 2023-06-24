org 0

djnz 15
jr 55
jr nz, 55
jr z, 55
jr nc, 55
jr c, 55

label_rew1:  nop
  djnz label_rew1
  djnz label_fwd1
label_fwd1:  nop

label_rew2:  nop
  jr label_rew2
  jr label_fwd2
label_fwd2:  nop

label_rew3:  nop
  jr nz, label_rew3
  jr nz, label_fwd3
label_fwd3:  nop

label_rew4:  nop
  jr z, label_rew4
  jr z, label_fwd4
label_fwd4:  nop

label_rew5:  nop
  jr nc, label_rew5
  jr nc, label_fwd5
label_fwd5:  nop

label_rew6:  nop
  jr nc, label_rew6
  jr nc, label_fwd6
label_fwd6:  nop
