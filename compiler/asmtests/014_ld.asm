ld a, b
ld b, b
ld c, b
ld d, b
ld e, b
ld h, b
ld l, b
ld d, a
ld d, b
ld d, c
ld d, d
ld d, e
ld d, h
ld d, l
ld a, 88
ld b, 88
ld c, 88
ld d, 88
ld e, 88
ld h, 88
ld l, 88
ld a, (hl)
ld b, (hl)
ld c, (hl)
ld d, (hl)
ld e, (hl)
ld h, (hl)
ld l, (hl)
ld a, (ix + 5)
ld b, (ix - 5)
ld c, (iy + 5)
ld d, (iy - 5)
ld e, (ix + 5)
ld h, (iy + 5)
ld l, (ix - 10)
ld (hl),a
ld (hl),b
ld (hl),c
ld (hl),d
ld (hl),e
ld (hl),h
ld (hl),l
ld (hl), 45
ld (ix+10), 78
ld (ix - 20), 78
ld (iy +30), 78
ld (iy- 40), 78
ld (ix-10), a
ld (ix-10), b
ld (ix-10), c
ld (ix-10), d
ld (ix-10), e
ld (ix-10), h
ld (ix-10), l
;ld a,(bc)
;ld a,(de)
ld a,(1234h)
ld (bc),a
ld (de),a
ld (4321h),a
ld i,a
ld r,a
ld a,i
ld a,r
ld bc, 10000
ld de, 10000
ld hl, 10000
ld sp, 10000
;ld ix, 35000
;ld iy, 53000
ld bc, (20000)
ld de, (20000)
ld hl, (20000)
ld sp, (20000)
;ld ix, (10)
;ld iy, (100)
ld (1000), bc
ld (1000), de
ld (1000), hl
ld (1000), sp
ld (1000), iy
ld (1000), ix
ld sp,hl
ld sp,ix
ld sp,iy
