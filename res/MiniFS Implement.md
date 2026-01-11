### 1 Introduction

In this assignment, we need to implement:
- A mini file system
- Operations in this file system

#### 1.1 Contents:
1. Structure Design
2. File Operation in File System
3. Implementation Details
4. Demonstration
5. Contributions
### 2 The structure of MiniFS

A file system requires at least:
1. **Virutal Disk Image**: The raw strorage medium (a file, `disk.img`).
2. **VCB(Volume Control Block)**
3. **FCB(File Control Block)**
4. **File and Directory**
#### 2.1 Partition and Volume
Take an example:
1. We buy a disk (HDD/SSD)
2. Make partitions (System/Data)
3. Format the partitions with specific File System (NTFS/ExFAT)
4. After formatting, we got volumes.
#### 2.2 Virtual Disk Image
- We have a file disk.img 
- Assume the virtual disk has 100Kb (Fixed size).
- There's 100 blocks in disk image, and each of they is 1Kb
![[Pasted image 20251111225936.png]]
#### 2.3 VCB
- a VCB contains information of the volume, it can be custom, we use the following for example:

|**field**|**calculation**|**value**|
|---|---|---|
|block_size||1024|
|block_count||100|
|fcb_size||32|
|bmap_start_block||1|
|bmap_end_block||1|
|fmap_start_block||2|
|fmap_end_block||2|
|fcb_start_block||3|
|fcb_end_block||9|
|fcb_count|(fcb_end_block - fcb_start_block + 1) * (block_size / fcb_size)|224|
|dbp_count||4|
|dentry_size||64|
|dentry_count|block_size * dbp_count / dentry_size|64|
- It is called superblock in UFS, while master file table in NTFS.

#### 2.4 FCB(inode)
- a FCB contains infomation of the file, we use 4 dbp in this sample

| Field                      | Value/Description (Example) |
| -------------------------- | --------------------------- |
| **Identifier Number**      | 0-224(fcb_count)            |
| **File Size**              | 0-4 KB (4096 bytes)         |
| **4 direct block pointer** | Block n1,n2,n3,n4           |


![[Pasted image 20251211233454.png]]
#### 2.5 File and Directory
In UNIX, a directory is treads as a special type of file, both of they have their own inode.

- File Structure (the contain of file blocks)
	- File Type: Image, document and executable file.
	- We just implement plain text file.
- Directory Structure 
	- Dentry: Records file name and number of inode.
	- Tree-Structured Directory
![[Pasted image 20251220203938.png]]
	- Every directory has the dentries:
		- **Itself**: "."
		- **Parent Directory**: "..", except root directory.
	

#### 2.6 Block map & FCB map
Stores the usage of blocks and FCB.
![[Pasted image 20251211233507.png]]
#### 2.7 Summary: Disk Layout
To implement a simple file system, we use the following variables:

| Block(s)           | Type                  | Rationale                                               |
| ------------------ | --------------------- | ------------------------------------------------------- |
| **Block 0**        | Superblock (VCB)      | Volume-wide metadata.                                   |
| **Block 1**        | Free block<br>        | Tracks usage of the 100 blocks.<br>                     |
| **Block 2**        | Free inode            | Tracks usage of the 100 inodes.                         |
| **Block 3 to 10**  | **FCB / Inode Table** | **One block stores MANY FCBs.**                         |
| **Block 11 to 99** | Data Blocks           | Stores the file and directory contents. (90/4=22 files) |

![[Pasted image 20260111110658.png]]

### 3 File Operation in File System
#### 3.1 Disk Utility
In every OS, we use disk utility to make a partition for a disk, and format a partition.
In our project, we use `mkfs` to:
- Write the VCB (Superblock) to Block 0 with total block counts, etc.
- Initialize the Bitmap (setting all data blocks to 'free').
- Initialize the FCB Table (setting all FCB slots to 'free' or 'unused').
- Create the root directory by allocating the first FCB and its first data block.
#### 3.2 Custom shell 
In every OS, we can create and access file via shell.
In this case, we use a C program to emulate a shell. Just use C Library.

#### 3.3 Shell Commands
| **Command**   | **Usage**             | **Description**                                               |
| ------------- | --------------------- | ------------------------------------------------------------- |
| **mkdir**     | `mkdir <path>`        | Creates a new directory at the specified path.                |
| **touch**     | `touch <path>`        | Creates a new empty file at the specified path.               |
| **ls**        | `ls [path]`           | Lists the contents of a directory.                            |
| **cd**        | `cd <path>`           | Changes the current working directory to the specified path.  |
| **pwd**       | `pwd`                 | Prints the absolute path of the current working directory.    |
| **rm**        | `rm [options] <path>` | Removes files or directories.                                 |
|               | `-r`                  | Removes directories and their contents recursively.           |
|               | `-f`                  | Forces removal without prompting for confirmation.            |
|               | `-rf`                 | Forces recursive removal of directories and contents.         |
| **cat**       | `cat <path>`          | Displays the content of a file.                               |
| **edit file** |                       | Modifies file content using redirection operators.            |
|               | `>>`                  | **Append**: Adds text to the end of the file.                 |
|               | `>`                   | **Override**: Replaces the entire file content with new text. |
| **stat**      | `stat <path>`         | Displays inode information (size, inode number, blocks used). |
| **exit**      | `exit`                | Terminates the current shell session or program.              |

### 4 Implementation Details
#### 4.1 Header File - Structs and Functions

**Struct**
We define FCB, VCB, Dentry to structs, and load FCB and FCB on program start.
- `uint32_t`: Force use 32 bit unsigned integer.  
- `#pragma pack...`: Memory alignment
```C
typedef struct {
    uint32_t block_size;
    uint32_t block_count;
    uint32_t fcb_size;

    uint32_t bmap_start_block;
    uint32_t bmap_end_block;

    uint32_t fmap_start_block;
    uint32_t fmap_end_block;

    uint32_t fcb_start_block;
    uint32_t fcb_end_block;
    uint32_t fcb_count;
} VCB;

typedef struct  {
    uint32_t is_dir; //4 byte
    uint32_t file_size; //4 byte
    uint32_t dbp[4]; //16 byte
    uint8_t unused[8];     
} FCB; 

#pragma pack(push, 1)
typedef struct {
    uint32_t file_id; //4byte
    char file_name[60]; //60byte
} Dentry;
#pragma pack(pop)
```

**Functions**
To simplify development and avoid code duplication, we write some common functions for reuse.  
```C
bool find_file_name_by_id(uint32_t dir_id, uint32_t file_id, char* file_name);
bool get_file_dentry(uint32_t dir_id, char* file_name, Dentry* dentry);
void print_dentry(Dentry* dentry);
void fs_delete_dentry(uint32_t dir_id, uint32_t index);
void fs_write_dentry(uint32_t dir_id,Dentry *new_dentry);
uint32_t fmap_new();
uint32_t bmap_new();
Dentry fs_read_dentry(uint32_t dir_id, uint32_t index);
void fs_write_fcb(uint32_t id,FCB *source_fcb);
FCB fs_read_fcb(uint32_t id);
uint32_t fs_read_uint32(uint32_t block, uint32_t offset);
uint8_t fs_read_char(uint32_t block, uint32_t offset);
void fs_write_uint32(uint32_t block, uint32_t offset, uint32_t data);
void fs_write_char(uint32_t block, uint32_t offset, char data);
void vcb_load();
void vcb_save();
void bmap_load();
void bmap_save();
void fmap_load();
void fmap_save();
void debug_dump();
uint32_t find_file(uint32_t dir_id, char* file_name);
```

#### 4.2 Big-Endian and Little-Endian 
The purpose of a miniFS: we want to store datas in Disk rather than RAM.
- Big-Endian: network, human-readable, use `xxd` to read binary file. 
- Little-Endian: most CPU do

In `fs_write_uint32(), minifs_ops.c`
```C
for (int boffset = 24; boffset >= 0; boffset-=8){
        fputc((data>>boffset) & 0xFF, fp);
    }
```

- Finally use `ntohl()` and `htonl()` in `winsock2.h` (win32) or`<arpa/inet.h>` (unix)
#### 4.3 Shell Command Resolve
**Shell Command Resolve**
In Unix, a command is represented as `command [-options] [parameter]`.
In our custom shell, for example:  To split the string`rm -r /dir1/file\0`, use `strtok(input, " ")` to split string, `strtok(NULL, " ")` for next.
```bash
rm\0-r\0/dir1/file\0
^   ^   ^
command;options;parameter
```
**Path Resolve**
Same as command, use `strtok(input, "/")` split parameter to path token.
#### 4.4 Dentry
Due to a directory only contents dentries, we have to traverse entire blocks to find specific dentry, can be improve with memory buffer in future.
- Get file id by name: `find_file()` will return an index, and use `fs_read_dentry()` with the index to get the dentry.
- Get file name by id: `find_file_name_by_id()`, newer function to simplify the logic.
![[Pasted image 20260111132051.png]]
### 5 Contirbutions
**Members**
- **蕭宏均 陳泰頤**: 重點開發主力
- **鄒昱宸 李冠緯 黃婧絜**: 協力 
- **陳裕勛**: 完全沒出現，寄信不回上課也找不到人

**Tasks:**
1. 虛擬磁碟結構設計，完成 mkfs (✅:完成 ☑️:分配中 ❌:未完成)

| Items                 | 黃婧絜   | 鄒昱宸   | 蕭宏均   | 陳泰頤 | 李冠緯   | Description                                                                   |
| --------------------- | ----- | ----- | ----- | --- | ----- | ----------------------------------------------------------------------------- |
| Design Disk Structure |       |       | ✅     |     |       |                                                                               |
| mkfs mkVCB()          | ✅<br> | ✅<br> |       |     |       |                                                                               |
| mkfs mkBitmap()       | ✅<br> | ✅<br> |       |     |       |                                                                               |
| mkfs mkFCB()          |       |       | ✅<br> |     |       |                                                                               |
| mkfs mkRoot()         |       |       |       |     | ✅<br> | Create first FCB to root dirctory.                                            |
| mkfs.exe              |       |       | ✅️    |     |       | Merge these functions to an executable file for initializing the file system. |


2. 實作 inode 與檔案讀寫，完成 touch / append / cat

| Items                     | 黃婧絜   | 鄒昱宸   | 蕭宏均 | 陳泰頤 | Description                                                                                                                                                   |
| ------------------------- | ----- | ----- | --- | --- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| touch                     |       |       | ✅   |     | 1. id = mkFCB()<br>2. add dentry(file name and id) to root directory(block10)                                                                                 |
| append<br>and<br>override | ✅     | ✅     |     | ✅   | Append text to the file.<br>1. find the file name in the directory and get its inode id<br>2. get the DBPs from the inode<br>3. write content to these blocks |
| cat                       | ✅<br> | ✅<br> |     | ✅   | Show the content of the file.                                                                                                                                 |


3. 實作目錄與路徑解析，完成 mkdir / ls / stat / rm / cd / pwd


| Items        | 蕭宏均 | 陳泰頤 | Description |
| ------------ | --- | --- | ----------- |
| mkdir        | ✅   |     |             |
| ls           |     | ✅   |             |
| pwd          | ✅   |     |             |
| rm           |     | ✅   |             |
| cd           |     | ✅   |             |
| stat         | ✅   |     |             |
| path resolve | ✅   | ✅   |             |
| shell        | ✅   |     |             |


4. 整合成 shell，設計測試流程，繳交程式與說明文件


| Items         | 蕭宏均   | 陳泰頤   | Description |
| ------------- | ----- | ----- | ----------- |
| Documentation | ✅     |       |             |
| Presentation  | ✅<br> |       |             |
| Demo          |       | ✅<br> |             |
| Test Script   |       | ✅     |             |


