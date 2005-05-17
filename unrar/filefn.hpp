#ifndef _RAR_FILEFN_
#define _RAR_FILEFN_

enum MKDIR_CODE {MKDIR_SUCCESS,MKDIR_ERROR,MKDIR_BADPATH};

MKDIR_CODE MakeDir(const char *Name,const wchar *NameW,uint Attr);
void CreatePath(const char *Path,const wchar *PathW,bool SkipLastName);
void SetDirTime(const char *Name,RarTime *ftm,RarTime *ftc,RarTime *fta);
bool IsRemovable(const char *Name);
Int64 GetFreeDisk(const char *Name);
bool FileExist(const char *Name,const wchar *NameW=NULL);
bool WildFileExist(const char *Name,const wchar *NameW=NULL);
bool IsDir(uint Attr);
bool IsUnreadable(uint Attr);
bool IsLabel(uint Attr);
bool IsLink(uint Attr);
void SetSFXMode(const char *FileName);
void EraseDiskContents(const char *FileName);
bool IsDeleteAllowed(uint FileAttr);
void PrepareToDelete(const char *Name,const wchar *NameW=NULL);
uint GetFileAttr(const char *Name,const wchar *NameW=NULL);
bool SetFileAttr(const char *Name,const wchar *NameW,uint Attr);
void ConvertNameToFull(const char *Src,char *Dest);
void ConvertNameToFull(const wchar *Src,wchar *Dest);
char* MkTemp(char *Name);


uint CalcFileCRC(File *SrcFile,Int64 Size=INT64ERR);
bool RenameFile(const char *SrcName,const wchar *SrcNameW,const char *DestName,const wchar *DestNameW);
bool DelFile(const char *Name);
bool DelFile(const char *Name,const wchar *NameW);
bool DelDir(const char *Name);
bool DelDir(const char *Name,const wchar *NameW);

#if defined(_WIN_32) && !defined(_WIN_CE)
bool SetFileCompression(char *Name,wchar *NameW,bool State);
#endif

#endif
