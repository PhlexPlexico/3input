.3ds
.open "code.bin", "code-edited.bin", 0x00100000

patch:
//increase handle count
.org 0x111064
.dcb 4
.org 0x111074
.dcb 4

.org 0x10B634
GetLastPressedPA:
push {lr}
mov r1, #0
ldr r0, =0x113344 // latestkeys
bl svcConvertVAToPA
pop {pc}
nop
.pool

svcConvertVAToPA:
swi 0x90
bx lr

GetStatePA:
.org 0x10B65C
push {lr}
mov r1, #0
ldr r0, =0x1132C9 // initalized
bl svcConvertVAToPA
pop {pc}
.pool

.org 0x1058CC
add r0, sp, #0x60 // rawdata
add r1, sp, #0x68 // v34
bl updatem_latestkeys

.org 0x10AD40
// Called from the CPP/ExtHID packet handler (hook at 0x1058CC) with
// r0 = raw packet word, r1 = converted {x, y} (u16 each, 4 bytes apart).
// Same job as before (latest ZL/ZR/C-Stick word at 0x113344), plus: the adjusted
// C-Stick x/y are also stored in the newest entry of the ir:rst shared memory
// ring ([0x113340] = ring base, +0x10 = index, entries at +0x18, x/y at +0xC/+0xE),
// so the analog values are readable from a normal ir:rst shared memory mapping.
updatem_latestkeys:
push {r4, lr}
sub sp, sp, #0x14
str r0, [sp, #0x10]

ldrh r3, [r1]
strh r3, [sp]
ldrh r3, [r1, #4]
strh r3, [sp, #2]
mov r0, sp
mov r1, sp
mov r2, sp
bl 0x10F30C

mov r0, sp
bl 0x10F208
mov r1, #0xF000000
and r0, r1, r0, lsr #4

ldr r3, [sp, #0x10]
ldr r3, [r3]
tst r3, #0x200
orrne r0, r0, #0x8000
tst r3, #0x400
orrne r0, r0, #0x4000
ldr r4, =0x113344
str r0, [r4]

ldr r2, =0x113340
ldr r2, [r2]
cmp r2, #0
beq done
ldr r3, [r2, #0x10]
cmp r3, #7
bhi done
add r2, r2, r3, lsl #4
ldrsh r0, [sp]
ldrsh r1, [sp, #2]
strh r0, [r2, #0x24]
strh r1, [r2, #0x26]

done:
add sp, sp, #0x14
pop {r4, pc}
.pool

RemoveZLZR:
.org 0x105884
mov r1, #0
stmea sp, {r1, r2}
mov r3, #0

.org 0x1048D0 // Remove ZL/ZR from irrstSamplingThread
orr r1, r1, #0x20000
.org 0x1048E4
orr r0, r0, #0x10000

.close