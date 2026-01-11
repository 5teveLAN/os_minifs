### Compile

`make`

### Create disk image

- Linux:`./mkfs disk.img 102400`
- Windows:`./mkfs.exe disk.img 102400`

### Start Shell

- Linux:`./shell disk.img`
- Windows:`./shell.exe disk.img`

Using autotest script:

- Linux:`./shell disk.img < fulltest`
- Windows:`./shell.exe disk.img | fulltest`


### View content
1. Start from 10240 byte
```bash
# Linux/Git Bash
xxd -c 16 -g 4 -s 10240 disk.img | less

# Windows PowerShell (view bytes from offset 10240)
$bytes = [System.IO.File]::ReadAllBytes("disk.img")
$bytes[10240..11263] | Format-Hex

# Or simply view the entire file
Format-Hex disk.img | more
```

