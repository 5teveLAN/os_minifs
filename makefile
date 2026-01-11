# Cross-platform Makefile (Linux & Windows)
# 定義編譯器
CC = gcc
# 定義編譯參數：-Wall 開啟警告, -g 加入除錯資訊
CFLAGS = -Wall -g

# 預設額外庫（依平台加入）
LIBS =

# 偵測 Windows 環境（在 Windows CMD/PowerShell 下，環境變數 OS=Windows_NT）
ifeq ($(OS),Windows_NT)
	EXEEXT = .exe
	RM = del /Q
	MKFS_CMD = mkfs$(EXEEXT)
	# Windows 常需要定義 _WIN32 並連結 winsock
	CFLAGS += -D_WIN32
	LIBS += -lws2_32
else
	EXEEXT =
	RM = rm -f
	MKFS_CMD = ./mkfs$(EXEEXT)
endif

# 定義目標檔案（會自動帶上副檔名）
TARGETS = mkfs$(EXEEXT) shell$(EXEEXT)

# 定義中間產物
OBJS = minifs_ops.o

# 預設動作：編譯所有目標
all: $(TARGETS)

# 編譯 minifs_ops.o
minifs_ops.o: minifs_ops.c minifs_ops.h
	$(CC) $(CFLAGS) -c minifs_ops.c -o minifs_ops.o

# 編譯 mkfs
mkfs$(EXEEXT): mkfs.c $(OBJS)
	$(CC) $(CFLAGS) mkfs.c $(OBJS) $(LIBS) -o mkfs$(EXEEXT)

# 編譯 shell
shell$(EXEEXT): shell.c $(OBJS)
	$(CC) $(CFLAGS) shell.c $(OBJS) $(LIBS) -o shell$(EXEEXT)

# 建立 100MB 的磁碟鏡像檔 (測試用)
disk: mkfs$(EXEEXT)
	$(MKFS_CMD) disk.img 102400

# 清除編譯產物
clean:
	$(RM) $(TARGETS) $(OBJS) disk.img || true

# 偽目標，防止與檔名衝突
.PHONY: all clean disk
