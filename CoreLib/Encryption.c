#include <zlib.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include "Diagnostic.h"
#include "Encryption.h"
#include "FileIO.h"

typedef Int32 (*zlib_inflate_init_proc)(z_streamp, Int32, const Char*, Int32);
typedef Int32 (*zlib_inflate_proc)(z_streamp, Int32);
typedef Int32 (*zlib_inflate_end_proc)(z_streamp);

Int32 zlib_inflate_init_stub(z_streamp _0, Int32 _1, const Char* _2, Int32 _3) { return 0; }
Int32 zlib_inflate_stub(z_streamp _0, Int32 _1) { return 0; }
Int32 zlib_inflate_end_stub(z_streamp _0) { return 0; }

#ifdef _WIN32
static HINSTANCE Library = NULL;
#else
static void *Library = NULL;
#endif

static zlib_inflate_init_proc InflateInit = zlib_inflate_init_stub;
static zlib_inflate_proc Inflate = zlib_inflate_stub;
static zlib_inflate_end_proc InflateEnd = zlib_inflate_end_stub;

#define ARCHIVE_LIBRARY_VERSION "1.2.8"
#define ARCHIVE_CHUNK_SIZE      16384
#define ARCHIVE_HEADER_XOR      0x57676592
#define ARCHIVE_BUFFER_SIZE     1024

Bool EncryptionLoadLibrary() {
#ifdef _WIN32
    Library = LoadLibrary(TEXT("zlib.dll"));
    if (!Library) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return false;
    }

    InflateInit = (zlib_inflate_init_proc)GetProcAddress(Library, "inflateInit2_");
    Inflate = (zlib_inflate_proc)GetProcAddress(Library, "inflate");
    InflateEnd = (zlib_inflate_end_proc)GetProcAddress(Library, "inflateEnd");

    if (!InflateInit || !Inflate || !InflateEnd) {
        Error("GetProcAddress failed: %lu\n", GetLastError());
        FreeLibrary(Library);
        Library = NULL;
        return false;
    }
#else
    Library = dlopen("libz.so", RTLD_LAZY);
    if (!Library) {
        Char* Message = dlerror();
        Error("dlopen failed: %s\n", Message ? Message : "Unknown error");
        return false;
    }

    InflateInit = (zlib_inflate_init_proc)dlsym(Library, "inflateInit2_");
    Inflate = (zlib_inflate_proc)dlsym(Library, "inflate");
    InflateEnd = (zlib_inflate_end_proc)dlsym(Library, "inflateEnd");

    if (!InflateInit || !Inflate || !InflateEnd) {
        Char* Message = dlerror();
        Error("dlsym failed: %s\n", Message ? Message : "Unknown error");
        dlclose(Library);
        Library = NULL;
        return false;
    }
#endif

    return true;
}

Bool EncryptionUnloadLibrary() {
    Bool Success = true;
    if (Library) {
#ifdef _WIN32
        Success = FreeLibrary(Library);
#else
        Success = !dlclose(Library);
#endif
    }

    Library = NULL;
    return Success;
}

Bool InflateDecryptBuffer(
    UInt8* Source,
    Int32 SourceLength,
    UInt8** Destination,
    Int32* DestinationLength
) {
    z_stream Stream = { 0 };
    Stream.avail_in = 0;
    Stream.next_in = Z_NULL;
    Stream.avail_out = 0;
    Stream.next_out = Z_NULL;

    Int32 Status = InflateInit(&Stream, -15, ARCHIVE_LIBRARY_VERSION, sizeof(Stream));
    if (Status != Z_OK) return false;

    Int32 OutputIndex = 0;
    Int32 OutputLength = ARCHIVE_BUFFER_SIZE;
    UInt8* Output = malloc(OutputLength);
    if (!Output) goto error;

    Bool First = true;
    Int32 Index = sizeof(Int32);
    static UInt8 In[ARCHIVE_CHUNK_SIZE];
    static UInt8 Out[ARCHIVE_CHUNK_SIZE];

    do {
        Stream.avail_in = MIN(ARCHIVE_CHUNK_SIZE, SourceLength - Index);
        if (Stream.avail_in < 1) break;

        memcpy(In, &Source[Index], Stream.avail_in);
        Index += Stream.avail_in;

        if (First) {
            First = false;
            *(UInt32*)In ^= ARCHIVE_HEADER_XOR;
        }

        Stream.next_in = In;

        do {
            Stream.avail_out = ARCHIVE_CHUNK_SIZE;
            Stream.next_out = Out;

            Status = Inflate(&Stream, Z_NO_FLUSH);
            if (Status != Z_OK && Status != Z_STREAM_END) goto error;

            UInt32 Length = ARCHIVE_CHUNK_SIZE - Stream.avail_out;
            while (OutputIndex + Length >= (UInt32)OutputLength) {
                OutputLength *= 2;
            }

            UInt8* NewOutput = realloc(Output, OutputLength);
            if (!NewOutput) Fatal("Memory allocation failed!");
            Output = NewOutput;

            memcpy(&Output[OutputIndex], Out, Length);
            OutputIndex += Length;

        } while (Stream.avail_out < 1);

    } while (Status != Z_STREAM_END);

    InflateEnd(&Stream);

    *Destination = Output;
    *DestinationLength = OutputIndex;

    if (OutputIndex < OutputLength) {
        memset(&Output[OutputIndex], 0, OutputLength - OutputIndex);
    }

    return true;

error:
    InflateEnd(&Stream);

    if (Output) free(Output);

    *Destination = NULL;
    *DestinationLength = 0;

    return false;
}

Bool EncryptionDecryptFile(
    CString FilePath,
    UInt8** Destination,
    Int32* DestinationLength
) {
    UInt8* Source = NULL;
    Int32 SourceLength = 0;
    *Destination = NULL;
    *DestinationLength = 0;

    FileRef File = FileOpen(FilePath);
    if (!File) 
        goto error;

    if (!FileRead(File, &Source, &SourceLength)) 
        goto error;

    if (!InflateDecryptBuffer(Source, SourceLength, Destination, DestinationLength)) 
        goto error;

    FileClose(File);
    free(Source);

    return true;

error:
    if (File) FileClose(File);
    if (Source) free(Source);
    if (*Destination) free(*Destination);

    return false;
}
