# DS4 Authentication Reverse Engineering Notes

## Protocol

Input:

[NOUSE 1B] + [TID 1B] + [input challenge 32B]

Total: 34 bytes

Internal construction:

```
dword_20023B68 =
[v2 ^ sub_14836(), 4B]
+ [HIDWORD(v1) ^ v5[1], 4B]
+ [byte_1FFE0290, 64B]
+ [dword_1FFE0114, 4B]
+ [dword_1FFE00C8, 4B]

SHA1(dword_20023B68)

```

Second buffer:

```
table[TID] + challenge + SHA1(dword_20023B68)
```

Output:

```
input[0:2]
+ SHA1(dword_20023AEA)
+ SHA1(dword_20023B68)
```

Total response: 42 bytes

## I2C

Write: 0x60
Read:  0x61

This folder is for PSXCore DS4 authentication research.
