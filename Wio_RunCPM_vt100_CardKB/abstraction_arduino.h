#ifndef ABSTRACT_H
#define ABSTRACT_H
#include <Wire.h>

// CrdKB I2C アドレス
#define CARDKB_ADDR 0x5F

#ifdef PROFILE
#define printf(a, b) Serial.println(b)
#endif

#if defined ARDUINO_SAM_DUE || defined ADAFRUIT_GRAND_CENTRAL_M4 || SEEED_WIO_TERMINAL
#define HostOS 0x01
#endif
#ifdef CORE_TEENSY
#define HostOS 0x04
#endif
#ifdef ESP32
#define HostOS 0x05
#endif
#ifdef _STM32_DEF_
#define HostOS 0x06
#endif

PROGMEM const uint8 KEY_TBL[48] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1c, 0x1f, 0x1b, 0x1d, 0x00, 
                                   0x00, 0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09, 0x0f, 0x10, 0x00, 
                                   0x00, 0x00, 0x01, 0x13, 0x04, 0x06, 0x07, 0x08, 0x0a, 0x0b, 0x0c, 0x00, 
                                   0x00, 0x00, 0x1a, 0x18, 0x03, 0x16, 0x02, 0x0e, 0x0d, 0x00, 0x00, 0x00};


/* Memory abstraction functions */
/*===============================================================================*/
bool _RamLoad(char* filename, uint16 address) {
	File f;
	bool result = false;

	if (f = SD.open(filename, FILE_READ)) {
		while (f.available())
			_RamWrite(address++, f.read());
		f.close();
		result = true;
	}
	return(result);
}

/* Filesystem (disk) abstraction fuctions */
/*===============================================================================*/
File rootdir, userdir;
#define FOLDERCHAR '/'

typedef struct {
	uint8 dr;
	uint8 fn[8];
	uint8 tp[3];
	uint8 ex, s1, s2, rc;
	uint8 al[16];
	uint8 cr, r0, r1, r2;
} CPM_FCB;

typedef struct {
	uint8 dr;
	uint8 fn[8];
	uint8 tp[3];
	uint8 ex, s1, s2, rc;
	uint8 al[16];
} CPM_DIRENTRY;

//#if defined board_teensy41
  static DirFat_t fileDirEntry;
//#else
//  static dir_t    fileDirEntry;
//#endif

File _sys_fopen_w(uint8* filename) {
	return(SD.open((char*)filename, O_CREAT | O_WRITE));
}

int _sys_fputc(uint8 ch, File& f) {
	return(f.write(ch));
}

void _sys_fflush(File& f) {
	f.flush();
}

void _sys_fclose(File& f) {
	f.close();
}

int _sys_select(uint8* disk) {
	uint8 result = FALSE;
	File f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open((char*)disk, O_READ)) {
		if (f.isDirectory())
			result = TRUE;
		f.close();
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

long _sys_filesize(uint8* filename) {
	long l = -1;
	File f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open((char*)filename, O_RDONLY)) {
		l = f.size();
		f.close();
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(l);
}

int _sys_openfile(uint8* filename) {
	File f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		f.dirEntry(&fileDirEntry);
		f.close();
		result = 1;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

int _sys_makefile(uint8* filename) {
	File f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_CREAT | O_WRITE);
	if (f) {
		f.close();
		result = 1;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

int _sys_deletefile(uint8* filename) {
	digitalWrite(LED, HIGH ^ LEDinv);
	return(SD.remove((char*)filename));
	digitalWrite(LED, LOW ^ LEDinv);
}

int _sys_renamefile(uint8* filename, uint8* newname) {
	File f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_WRITE | O_APPEND);
	if (f) {
    if (f.rename((char*)newname)) {
			f.close();
			result = 1;
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

#ifdef DEBUGLOG
void _sys_logbuffer(uint8* buffer) {
#ifdef CONSOLELOG
	puts((char*)buffer);
#else
	File f;
	uint8 s = 0;
	while (*(buffer + s))	// Computes buffer size
		++s;
	if (f = SD.open(LogName, O_CREAT | O_APPEND | O_WRITE)) {
		f.write(buffer, s);
		f.flush();
		f.close();
	}
#endif
}
#endif

bool _sys_extendfile(char* fn, unsigned long fpos)
{
	uint8 result = true;
	File f;
	unsigned long i;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open(fn, O_WRITE | O_APPEND)) {
		if (fpos > f.size()) {
			for (i = 0; i < f.size() - fpos; ++i) {
				if (f.write((uint8)0) != 1) {
					result = false;
					break;
				}
			}
		}
		f.close();
	} else {
		result = false;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_readseq(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File f;
	uint8 bytesread;
	uint8 dmabuf[BlkSZ];
	uint8 i;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		if (f.seek(fpos)) {
			for (i = 0; i < BlkSZ; ++i)
				dmabuf[i] = 0x1a;
			bytesread = f.read(&dmabuf[0], BlkSZ);
			if (bytesread) {
				for (i = 0; i < BlkSZ; ++i)
					_RamWrite(dmaAddr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			result = 0x01;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_writeseq(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (_sys_extendfile((char*)filename, fpos))
		f = SD.open((char*)filename, O_RDWR);
	if (f) {
		if (f.seek(fpos)) {
			if (f.write(_RamSysAddr(dmaAddr), BlkSZ))
				result = 0x00;
		} else {
			result = 0x01;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_readrand(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File f;
	uint8 bytesread;
	uint8 dmabuf[BlkSZ];
	uint8 i;
	long extSize;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		if (f.seek(fpos)) {
			for (i = 0; i < BlkSZ; ++i)
				dmabuf[i] = 0x1a;
			bytesread = f.read(&dmabuf[0], BlkSZ);
			if (bytesread) {
				for (i = 0; i < BlkSZ; ++i)
					_RamWrite(dmaAddr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			if (fpos >= 65536L * BlkSZ) {
				result = 0x06;	// seek past 8MB (largest file size in CP/M)
			} else {
				extSize = f.size();
				// round file size up to next full logical extent
				extSize = ExtSZ * ((extSize / ExtSZ) + ((extSize % ExtSZ) ? 1 : 0));
				if (fpos < extSize)
					result = 0x01;	// reading unwritten data
				else
					result = 0x04; // seek to unwritten extent
			}
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_writerand(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (_sys_extendfile((char*)filename, fpos)) {
		f = SD.open((char*)filename, O_RDWR);
	}
	if (f) {
		if (f.seek(fpos)) {
			if (f.write(_RamSysAddr(dmaAddr), BlkSZ))
				result = 0x00;
		} else {
			result = 0x06;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

static uint8 findNextDirName[13];
static uint16 fileRecords = 0;
static uint16 fileExtents = 0;
static uint16 fileExtentsUsed = 0;
static uint16 firstFreeAllocBlock;

uint8 _findnext(uint8 isdir) {
	File f;
	uint8 result = 0xff;
	bool isfile;
	uint32 bytes;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (allExtents && fileRecords) {
		_mockupDirEntry();
		result = 0;
	} else {
		while (f = userdir.openNextFile()) {
			f.getName((char*)&findNextDirName[0], 13);
			isfile = !f.isDirectory();
			bytes = f.size();
			f.dirEntry(&fileDirEntry);
			f.close();
			if (!isfile)
				continue;
			_HostnameToFCBname(findNextDirName, fcbname);
			if (match(fcbname, pattern)) {
				if (isdir) {
					// account for host files that aren't multiples of the block size
					// by rounding their bytes up to the next multiple of blocks
					if (bytes & (BlkSZ - 1)) {
						bytes = (bytes & ~(BlkSZ - 1)) + BlkSZ;
					}
					fileRecords = bytes / BlkSZ;
					fileExtents = fileRecords / BlkEX + ((fileRecords & (BlkEX - 1)) ? 1 : 0);
					fileExtentsUsed = 0;
					firstFreeAllocBlock = firstBlockAfterDir;
					_mockupDirEntry();
				} else {
					fileRecords = 0;
					fileExtents = 0;
					fileExtentsUsed = 0;
					firstFreeAllocBlock = firstBlockAfterDir;
				}
				_RamWrite(tmpFCB, filename[0] - '@');
				_HostnameToFCB(tmpFCB, findNextDirName);
				result = 0x00;
				break;
			}
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _findfirst(uint8 isdir) {
	uint8 path[4] = { '?', FOLDERCHAR, '?', 0 };
	path[0] = filename[0];
	path[2] = filename[2];
	if (userdir)
		userdir.close();
	userdir = SD.open((char*)path); // Set directory search to start from the first position
	_HostnameToFCBname(filename, pattern);
	fileRecords = 0;
	fileExtents = 0;
	fileExtentsUsed = 0;
	return(_findnext(isdir));
}

uint8 _findnextallusers(uint8 isdir) {
	uint8 result = 0xFF;
	char dirname[13];
	bool done = false;

	while (!done) {
		while (!userdir) {
			userdir = rootdir.openNextFile();
			if (!userdir) {
				done = true;
				break;
			}
			userdir.getName(dirname, sizeof dirname);
			if (userdir.isDirectory() && strlen(dirname) == 1 && isxdigit(dirname[0])) {
				currFindUser = dirname[0] <= '9' ? dirname[0] - '0' : toupper(dirname[0]) - 'A' + 10;
				break;
			}
			userdir.close();
		}
		if (userdir) {
			result = _findnext(isdir);
			if (result) {
				userdir.close();
			} else {
				done = true;
			}
		} else {
			result = 0xFF;
			done = true;
		}
	}
	return result;
}

uint8 _findfirstallusers(uint8 isdir) {
	uint8 path[2] = { '?', 0 };

	path[0] = filename[0];
	if (rootdir)
		rootdir.close();
	if (userdir)
		userdir.close();
	rootdir = SD.open((char*)path); // Set directory search to start from the first position
	strcpy((char*)pattern, "???????????");
	if (!rootdir)
		return 0xFF;
	fileRecords = 0;
	fileExtents = 0;
	fileExtentsUsed = 0;
	return(_findnextallusers(isdir));
}

uint8 _Truncate(char* filename, uint8 rc) {
	File f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_WRITE | O_APPEND);
	if (f) {
		if (f.truncate(rc * BlkSZ)) {
			f.close();
			result = 1;
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

void _MakeUserDir() {
	uint8 dFolder = cDrive + 'A';
	uint8 uFolder = toupper(tohex(userCode));

	uint8 path[4] = { dFolder, FOLDERCHAR, uFolder, 0 };

	digitalWrite(LED, HIGH ^ LEDinv);
	SD.mkdir((char*)path);
	digitalWrite(LED, LOW ^ LEDinv);
}

uint8 _sys_makedisk(uint8 drive) {
	uint8 result = 0;
	if (drive < 1 || drive>16) {
		result = 0xff;
	} else {
		uint8 dFolder = drive + '@';
		uint8 disk[2] = { dFolder, 0 };
		digitalWrite(LED, HIGH ^ LEDinv);
		if (!SD.mkdir((char*)disk)) {
			result = 0xfe;
		} else {
			uint8 path[4] = { dFolder, FOLDERCHAR, '0', 0 };
			SD.mkdir((char*)path);
		}
		digitalWrite(LED, LOW ^ LEDinv);
	}

	return(result);
}

/* Console abstraction functions */
/*===============================================================================*/
extern void printChar(char c);
extern bool canShowCursor;    // カーソル表示可能か？
extern void dispCursor(bool forceupdate);
extern void printString(const char *str);
extern void playBeep(const uint16_t Number, const uint8_t ToneNo, const uint16_t Duration);

static uint8 kbhit_char = 0;

int _kbhit(void) {
//  return(Serial.available());
  if (!kbhit_char)
  {
    if (Wire.requestFrom(CARDKB_ADDR, 1))
    {
      kbhit_char = Wire.read();
      switch (kbhit_char)
      {
      case 0x07:
  //    tone(SPK_PIN, 4000, 583);
        break;
      case 0x82:          // Fn-2 (Ctrl+@)
      case 0x86:          // Fn-6 (Ctrl+^)
      case 0x87:          // Fn-7 (Ctrl+\)
      case 0x88:          // Fn-8 (Ctrl+_)
      case 0x89:          // Fn-9 (Ctrl+[)
      case 0x8a:          // Fn-0 (Ctrl+])
      case 0x8d ... 0x96: // Fn-Q..P
      case 0x9a ... 0xa2: // Fn-A..L
      case 0xa6 ... 0xac: // Fn-Z..M
        kbhit_char = KEY_TBL[kbhit_char - 0x80];
        break;
      default:
        break;
      }
    }
  }
  if (canShowCursor || kbhit_char)
     dispCursor(kbhit_char);
  return kbhit_char;
}

uint8 _getch(void) {
//  while (!Serial.available());
//  return(Serial.read());
  while (!_kbhit());
  uint8 ch = kbhit_char;
  kbhit_char = 0;
  return(ch);
}

uint8 _getche(void) {
	uint8 ch = _getch();
//	Serial.write(ch);
  printChar(ch);
	return(ch);
}

void _putch(uint8 ch) {
//	Serial.write(ch);
//  printChar(ch);
  switch (ch) {
    case 0x07:
      playBeep(1, 12, 583);
      break;
    default:
      printChar(ch);
      break;
  }
}

void _clrscr(void) {
//	Serial.println("\e[H\e[J");
  printString("\e[H\e[2J");
}

#endif
