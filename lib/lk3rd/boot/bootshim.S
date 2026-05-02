#ifndef BOOT_IMAGE_TEXT_OFFSET
#define BOOT_IMAGE_TEXT_OFFSET LK3RD_BASE
#endif

#ifndef BOOT_IMAGE_FLAGS
#define BOOT_IMAGE_FLAGS 0
#endif

#ifndef LK3RD_COPY_SIZE
#define LK3RD_COPY_SIZE LK3RD_SIZE
#endif

_Head:
  b     _Start          /* Linux arm64 images start with a branch. */
  .long 0

_ImageTextOffset:
  .quad BOOT_IMAGE_TEXT_OFFSET

_ImageSize:
  .quad LK3RD_SIZE       /* Image Size */
  .quad BOOT_IMAGE_FLAGS /* Flags */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .ascii "ARM\x64"      /* ARM64 Magic */
  .long 0               /* Reserved */

_Lk3rdBase:
  .quad LK3RD_BASE

_Lk3rdSize:
  .quad LK3RD_SIZE

_Lk3rdCopySize:
  .quad LK3RD_COPY_SIZE

_Start:
  adr   x16, _Payload

  mov   x7, x1
  mov   x8, x2
  mov   x9, x3

  mov   x11, CREATE_FDT_POINTER
  cmp   x11, 1
  b.ne  skip_dtb
  mov   x10, FDT_POINTER_ADDRESS
  str   x0, [x10]

skip_dtb:
  mov   x4, x16
  ldr   x5, _Lk3rdBase
  cmp   x4, x5
  beq   _RestoreArgs
  ldr   x6, _Lk3rdCopySize

_CopyLoop:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyLoop
  ldr   x5, _Lk3rdBase
  dsb   sy
  ic    iallu
  dsb   sy
  isb

_RestoreArgs:
  mov   x1, x7
  mov   x2, x8
  mov   x3, x9

_Entry:
  br    x5

_Dead:
  b     _Dead           /* We should never get here */

.text
.align 4

_Payload:
  /* Your code will get ran right after this binary */
