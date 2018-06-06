#define _WIN32_WINNT 0x0501

#include <windows.h>

#include <stdio.h>

#define dump_buffersize_megs 16
#define dump_buffersize (dump_buffersize_megs * 1024 * 1024)
#define dump_workingsetsize ((dump_buffersize_megs + 1) * 1024 * 1024)

DWORD save(const wchar_t * source_device_name, const wchar_t * filename) {

  DWORD err;

  HANDLE hdevice, houtput;

  DWORD bytes_to_transfer, byte_count;

  GET_LENGTH_INFORMATION source_disklength;

  DISK_GEOMETRY source_diskgeometry;

  LARGE_INTEGER offset;

  OVERLAPPED overlapped;

  BYTE * buffer;

  if (!SetProcessWorkingSetSize(GetCurrentProcess(), dump_workingsetsize, dump_workingsetsize)) 
  {
    err = GetLastError();
    printf("Error %u trying to expand working set.\n", err);
    return err;
  }

  buffer = VirtualAlloc(NULL, dump_buffersize, MEM_COMMIT, PAGE_READWRITE);

  if (buffer == NULL)
  {
    err = GetLastError();
    printf("Error %u trying to allocate buffer.\n", err);
    return err;
  }

  if (!VirtualLock(buffer, dump_buffersize))
  {
    err = GetLastError();
    printf("Error %u trying to lock buffer.\n", err);
    return err;
  }

  hdevice = CreateFile
    (
    source_device_name,
    GENERIC_READ,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING,
    NULL
    );

  if (hdevice == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    fprintf(stderr, "Error %u opening input device.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    hdevice,
    FSCTL_LOCK_VOLUME,
    NULL,
    0,
    NULL,
    0,
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u locking input volume.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    hdevice,
    IOCTL_DISK_GET_DRIVE_GEOMETRY,
    NULL,
    0,
    &source_diskgeometry,
    sizeof(source_diskgeometry),
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u getting device geometry.\n", err);
    return err;
  }

  switch (source_diskgeometry.MediaType)
  {
  case Unknown:
  case RemovableMedia:
  case FixedMedia:

    if (!DeviceIoControl
      (
      hdevice,
      IOCTL_DISK_GET_LENGTH_INFO,
      NULL,
      0,
      &source_disklength,
      sizeof(source_disklength),
      &byte_count,
      NULL
      ))
    {
      err = GetLastError();
      fprintf(stderr, "Error %u getting input device length.\n", err);
      return err;
    }

    fprintf(stderr, "\nInput disk has %I64i bytes.\n\n", source_disklength.Length.QuadPart);
    break;

  default:

    source_disklength.Length.QuadPart = 
      source_diskgeometry.Cylinders.QuadPart *
      source_diskgeometry.TracksPerCylinder *
      source_diskgeometry.SectorsPerTrack *
      source_diskgeometry.BytesPerSector;

    fprintf(stderr, 
      "\n"
      "Input device appears to be a floppy disk.  WARNING: if this is not a\n"
      "floppy disk the calculated size will probably be incorrect, resulting\n"
      "in an incomplete copy.\n"
      "\n"
      "Input disk has %I64i bytes.\n"
      "\n", 
      source_disklength.Length.QuadPart);

    break;
  }

  houtput = CreateFile
    (
    filename,
    GENERIC_WRITE,
    0,
    NULL,
    CREATE_ALWAYS,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
    NULL
    );

  if (houtput == INVALID_HANDLE_VALUE)
  {
    err = GetLastError();
    fprintf(stderr, "Error %u creating output file.\n", err);
    return err;
  }

  offset.QuadPart = 0;
  overlapped.hEvent = 0;

  for (;;) 
  {
    overlapped.Offset = offset.LowPart;
    overlapped.OffsetHigh = offset.HighPart;

    if (source_disklength.Length.QuadPart - offset.QuadPart < dump_buffersize) 
    {
      bytes_to_transfer = (DWORD)(source_disklength.Length.QuadPart - offset.QuadPart);
      if (bytes_to_transfer == 0) break;
    }
    else
    {
      bytes_to_transfer = dump_buffersize;
    }

    if (!ReadFile(hdevice, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      printf("Error %u initiating read from input disk.\n", err);
      return err;
    }

    if (!GetOverlappedResult(hdevice, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u reading from input disk.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      err = GetLastError();
      printf("Internal error - partial read.  Last error code %u.\n", err);
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      if (byte_count == 0) return ERROR_INVALID_FUNCTION;
      bytes_to_transfer = byte_count;
    }

    if (!WriteFile(houtput, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      if (err != ERROR_IO_PENDING)
      {
        printf("Error %u initiating write to output file.\n", err);
        return err;
      }
    }

    if (!GetOverlappedResult(houtput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u writing to output file.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      printf("Internal error - partial write.\n");
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      return ERROR_INVALID_FUNCTION;
    }

    offset.QuadPart += bytes_to_transfer;
  }

  overlapped.Offset = offset.LowPart;
  overlapped.OffsetHigh = offset.HighPart;

  if (!ReadFile(hdevice, buffer, source_diskgeometry.BytesPerSector, NULL, &overlapped)) 
  {
    err = GetLastError();
    if (err == ERROR_HANDLE_EOF)
    {
      printf("Save successfully completed.\n");      
      return 0;
    }
    printf("Error %u initiating read from input disk past end of file.\n", err);
    return err;
  }

  if (!GetOverlappedResult(hdevice, &overlapped, &byte_count, TRUE)) 
  {
    err = GetLastError();
    if (err == ERROR_HANDLE_EOF)
    {
      printf("Save successfully completed.\n");      
      return 0;
    }
    printf("Error %u reading from input disk past end of file.\n", err);
    return err;
  }

  if (byte_count == 0)
  {
    printf("Save successfully completed.\n"); 
    return 0;
  }

  printf("WARNING: the expected amount of data was successfully copied,\n"
         "but end of file not detected on input disk.  The copy might\n"
         "not be complete.");

  return ERROR_MORE_DATA;

}

DWORD write(const wchar_t * filename, const wchar_t * target_device_name) {

  DWORD err;

  HANDLE hinput, houtput;

  WIN32_FILE_ATTRIBUTE_DATA fad;

  DWORD bytes_to_transfer, byte_count;

  LARGE_INTEGER filelength;

  GET_LENGTH_INFORMATION target_disklength;

  DISK_GEOMETRY target_diskgeometry;

  LARGE_INTEGER transfer_length;

  LARGE_INTEGER offset;

  OVERLAPPED overlapped;

  BYTE * buffer;

  if (!SetProcessWorkingSetSize(GetCurrentProcess(), dump_workingsetsize, dump_workingsetsize)) 
  {
    err = GetLastError();
    printf("Error %u trying to expand working set.\n", err);
    return err;
  }

  buffer = VirtualAlloc(NULL, dump_buffersize, MEM_COMMIT, PAGE_READWRITE);

  if (buffer == NULL)
  {
    err = GetLastError();
    printf("Error %u trying to allocate buffer.\n", err);
    return err;
  }

  if (!VirtualLock(buffer, dump_buffersize))
  {
    err = GetLastError();
    printf("Error %u trying to lock buffer.\n", err);
    return err;
  }

  if (!GetFileAttributesEx(filename, GetFileExInfoStandard, &fad)) 
  {
    err = GetLastError();
    fprintf(stderr, "Error %u reading input file attributes.\n", err);
    return err;
  }

  filelength.HighPart = fad.nFileSizeHigh;
  filelength.LowPart = fad.nFileSizeLow;

  fprintf(stderr, "\nInput file has %I64i bytes.\n", filelength.QuadPart);

  hinput = CreateFile
    (
    filename,
    GENERIC_READ,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
    NULL
    );

  if (hinput == INVALID_HANDLE_VALUE)
  {
    err = GetLastError();
    fprintf(stderr, "Error %u opening input file.\n", err);
    return err;
  }

  houtput = CreateFile
    (
    target_device_name,
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING,
    NULL
    );

  if (houtput == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    fprintf(stderr, "Error %u opening output device.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    houtput,
    FSCTL_LOCK_VOLUME,
    NULL,
    0,
    NULL,
    0,
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u locking volume.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    houtput,
    IOCTL_DISK_GET_DRIVE_GEOMETRY,
    NULL,
    0,
    &target_diskgeometry,
    sizeof(target_diskgeometry),
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u getting output device geometry.\n", err);
    return err;
  }

  switch (target_diskgeometry.MediaType)
  {
  case Unknown:
  case RemovableMedia:
  case FixedMedia:

    if (!DeviceIoControl
      (
      houtput,
      IOCTL_DISK_GET_LENGTH_INFO,
      NULL,
      0,
      &target_disklength,
      sizeof(target_disklength),
      &byte_count,
      NULL
      ))
    {
      err = GetLastError();
      fprintf(stderr, "Error %u getting output device length.\n", err);
      return err;
    }

    fprintf(stderr, "Output disk has %I64i bytes.\n\n", target_disklength.Length.QuadPart);
    break;

  default:

    target_disklength.Length.QuadPart = 
      target_diskgeometry.Cylinders.QuadPart *
      target_diskgeometry.TracksPerCylinder *
      target_diskgeometry.SectorsPerTrack *
      target_diskgeometry.BytesPerSector;

    fprintf(stderr, 
      "\n"
      "Output device appears to be a floppy disk.  WARNING: if this is not a\n"
      "floppy disk the calculated output device size is probably incorrect,\n"
      "which might result in an incomplete copy.\n"
      "\n"
      "Output disk has %I64i bytes.\n"
      "\n", 
      target_disklength.Length.QuadPart);

    break;
  }

  if (filelength.QuadPart == target_disklength.Length.QuadPart)
  {
    transfer_length.QuadPart = filelength.QuadPart;
  }
  else if (filelength.QuadPart < target_disklength.Length.QuadPart)
  {
    fprintf(stderr, "Image is smaller than target.  Part of the target will not be written to.\n\n");
    transfer_length.QuadPart = filelength.QuadPart;
  }
  else
  {
    fprintf(stderr, "Image is larger than target.  Part of the image will not be copied.\n\n");
    transfer_length.QuadPart = target_disklength.Length.QuadPart;
  }

  offset.QuadPart = 0;
  overlapped.hEvent = 0;

  for (;;) 
  {
    overlapped.Offset = offset.LowPart;
    overlapped.OffsetHigh = offset.HighPart;

    if (transfer_length.QuadPart - offset.QuadPart < dump_buffersize) 
    {
      bytes_to_transfer = (DWORD)(transfer_length.QuadPart - offset.QuadPart);
      if (bytes_to_transfer == 0) break;
    }
    else
    {
      bytes_to_transfer = dump_buffersize;
    }

    if (!ReadFile(hinput, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      if (err != ERROR_IO_PENDING)
      {
        printf("Error %u initiating read from input file.\n", err);
        return err;
      }
    }

    if (!GetOverlappedResult(hinput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u reading from input file.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      err = GetLastError();
      printf("Internal error - partial read.  Last error code %u.\n", err);
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      if (byte_count == 0) return ERROR_INVALID_FUNCTION;
      bytes_to_transfer = byte_count;
    }

    if (!WriteFile(houtput, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      if (err != ERROR_IO_PENDING)
      {
        printf("Error %u initiating write to output disk.\n", err);
        return err;
      }
    }

    if (!GetOverlappedResult(houtput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u writing to output disk.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      printf("Internal error - partial write.\n");
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      return ERROR_INVALID_FUNCTION;
    }

    offset.QuadPart += bytes_to_transfer;
  }

  printf("Write successfully completed.\n");
  return 0;
}

DWORD clone(const wchar_t * source_device_name, const wchar_t * target_device_name) {

  DWORD err;

  HANDLE hinput, houtput;

  DWORD bytes_to_transfer, byte_count;

  GET_LENGTH_INFORMATION source_disklength;

  DISK_GEOMETRY source_diskgeometry;

  GET_LENGTH_INFORMATION target_disklength;

  DISK_GEOMETRY target_diskgeometry;

  LARGE_INTEGER transfer_length;

  LARGE_INTEGER offset;

  OVERLAPPED overlapped;

  BYTE * buffer;

  DWORD result;

  if (!SetProcessWorkingSetSize(GetCurrentProcess(), dump_workingsetsize, dump_workingsetsize)) 
  {
    err = GetLastError();
    printf("Error %u trying to expand working set.\n", err);
    return err;
  }

  buffer = VirtualAlloc(NULL, dump_buffersize, MEM_COMMIT, PAGE_READWRITE);

  if (buffer == NULL)
  {
    err = GetLastError();
    printf("Error %u trying to allocate buffer.\n", err);
    return err;
  }

  if (!VirtualLock(buffer, dump_buffersize))
  {
    err = GetLastError();
    printf("Error %u trying to lock buffer.\n", err);
    return err;
  }

  hinput = CreateFile
    (
    source_device_name,
    GENERIC_READ,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING,
    NULL
    );

  if (hinput == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    fprintf(stderr, "Error %u opening input device.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    hinput,
    FSCTL_LOCK_VOLUME,
    NULL,
    0,
    NULL,
    0,
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u locking input volume.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    hinput,
    IOCTL_DISK_GET_DRIVE_GEOMETRY,
    NULL,
    0,
    &source_diskgeometry,
    sizeof(source_diskgeometry),
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u getting device geometry.\n", err);
    return err;
  }

  switch (source_diskgeometry.MediaType)
  {
  case Unknown:
  case RemovableMedia:
  case FixedMedia:

    if (!DeviceIoControl
      (
      hinput,
      IOCTL_DISK_GET_LENGTH_INFO,
      NULL,
      0,
      &source_disklength,
      sizeof(source_disklength),
      &byte_count,
      NULL
      ))
    {
      err = GetLastError();
      fprintf(stderr, "Error %u getting input device length.\n", err);
      return err;
    }

    fprintf(stderr, "\nInput disk has %I64i bytes.\n", source_disklength.Length.QuadPart);
    break;

  default:

    source_disklength.Length.QuadPart = 
      source_diskgeometry.Cylinders.QuadPart *
      source_diskgeometry.TracksPerCylinder *
      source_diskgeometry.SectorsPerTrack *
      source_diskgeometry.BytesPerSector;

    fprintf(stderr, 
      "\n"
      "Input device appears to be a floppy disk.  WARNING: if this is not a\n"
      "floppy disk the calculated disk size is probably incorrect, resulting\n"
      "in an incomplete copy.\n"
      "\n"
      "Input disk has %I64i bytes.\n",
      source_disklength.Length.QuadPart);

    break;
  }

  houtput = CreateFile
    (
    target_device_name,
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING,
    NULL
    );

  if (houtput == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    fprintf(stderr, "Error %u opening output device.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    houtput,
    FSCTL_LOCK_VOLUME,
    NULL,
    0,
    NULL,
    0,
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u locking output volume.\n", err);
    return err;
  }

  if (!DeviceIoControl
    (
    houtput,
    IOCTL_DISK_GET_DRIVE_GEOMETRY,
    NULL,
    0,
    &target_diskgeometry,
    sizeof(target_diskgeometry),
    &byte_count,
    NULL
    ))
  {
    err = GetLastError();
    fprintf(stderr, "Error %u getting output device geometry.\n", err);
    return err;
  }

  switch (target_diskgeometry.MediaType)
  {
  case Unknown:
  case RemovableMedia:
  case FixedMedia:

    if (!DeviceIoControl
      (
      houtput,
      IOCTL_DISK_GET_LENGTH_INFO,
      NULL,
      0,
      &target_disklength,
      sizeof(target_disklength),
      &byte_count,
      NULL
      ))
    {
      err = GetLastError();
      fprintf(stderr, "Error %u getting output device length.\n", err);
      return err;
    }

    fprintf(stderr, "Output disk has %I64i bytes.\n\n", target_disklength.Length.QuadPart);
    break;

  default:

    target_disklength.Length.QuadPart = 
      target_diskgeometry.Cylinders.QuadPart *
      target_diskgeometry.TracksPerCylinder *
      target_diskgeometry.SectorsPerTrack *
      target_diskgeometry.BytesPerSector;

    fprintf(stderr, 
      "\n"
      "Output device appears to be a floppy disk.  WARNING: if this is not a\n"
      "floppy disk the calculated output device size is probably incorrect,\n"
      "which might result in an incomplete copy.\n"
      "\n"
      "Output disk has %I64i bytes.\n"
      "\n", 
      target_disklength.Length.QuadPart);

    break;
  }

  if (source_disklength.Length.QuadPart == target_disklength.Length.QuadPart)
  {
    transfer_length.QuadPart = source_disklength.Length.QuadPart;
  }
  else if (source_disklength.Length.QuadPart < target_disklength.Length.QuadPart)
  {
    printf("Input shorter than output.  Part of the output disk will not be written to.\n\n");
    transfer_length.QuadPart = source_disklength.Length.QuadPart;
  }
  else
  {
    printf("Output shorter than input.  Copy will be truncated to output length.\n\n");
    transfer_length.QuadPart = target_disklength.Length.QuadPart;
  }

  offset.QuadPart = 0;
  overlapped.hEvent = 0;

  for (;;) 
  {
    overlapped.Offset = offset.LowPart;
    overlapped.OffsetHigh = offset.HighPart;

    if (transfer_length.QuadPart - offset.QuadPart < dump_buffersize) 
    {
      bytes_to_transfer = (DWORD)(transfer_length.QuadPart - offset.QuadPart);
      if (bytes_to_transfer == 0) break;
    }
    else
    {
      bytes_to_transfer = dump_buffersize;
    }

    if (!ReadFile(hinput, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      printf("Error %u initiating read from input file.\n", err);
      return err;
    }

    if (!GetOverlappedResult(hinput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u reading from input file.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      err = GetLastError();
      printf("Internal error - partial read.  Last error code %u.\n", err);
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      if (byte_count == 0) return ERROR_INVALID_FUNCTION;
      bytes_to_transfer = byte_count;
    }

    if (!WriteFile(houtput, buffer, bytes_to_transfer, NULL, &overlapped)) 
    {
      err = GetLastError();
      if (err != ERROR_IO_PENDING)
      {
        printf("Error %u initiating write to output disk.\n", err);
        return err;
      }
    }

    if (!GetOverlappedResult(houtput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      printf("Error %u writing to output disk.\n", err);
      return err;
    }

    if (byte_count != bytes_to_transfer)
    {
      printf("Internal error - partial write.\n");
      printf("bytes_to_transfer = %u; byte_count = %u.\n", bytes_to_transfer, byte_count);
      return ERROR_INVALID_FUNCTION;
    }

    offset.QuadPart += bytes_to_transfer;
  }

  if (transfer_length.QuadPart == source_disklength.Length.QuadPart)
  {
    overlapped.Offset = offset.LowPart;
    overlapped.OffsetHigh = offset.HighPart;

    if (!ReadFile(hinput, buffer, source_diskgeometry.BytesPerSector, NULL, &overlapped)) 
    {
      err = GetLastError();
      if (err == ERROR_HANDLE_EOF)
      {
        printf("Copy successfully completed.\n");      
        return 0;
      }
      printf("Error %u initiating read from input disk past end of file.\n", err);
      return err;
    }

    if (!GetOverlappedResult(hinput, &overlapped, &byte_count, TRUE)) 
    {
      err = GetLastError();
      if (err == ERROR_HANDLE_EOF)
      {
        printf("Copy successfully completed.\n");
        return 0;
      }
      printf("Error %u reading from input disk past end of file.\n", err);
      return err;
    }

    if (byte_count == 0)
    {
      printf("Copy successfully completed.\n"); 
      return 0;
    }

    printf("WARNING: the expected amount of data was successfully copied,\n"
           "but end of file not detected on input disk.  The copy might\n"
           "not be complete.");

    result = ERROR_MORE_DATA;
    return 0;
  }

  printf("Copy successfully completed.\n");
  return 0;
}

int wmain(int argc, wchar_t ** argv)
{
  if (argc < 4)
  {
    printf("Syntax: \n"
      "To save an image of a physical drive:\n"
      "diskimage /save \\\\.\\PhysicalDrive0 file.img\n"
      "diskimage /save \\\\.\\A: file.img\n"
      "To write from an image file to a physical drive:\n"
      "diskimage /write file.img \\\\.\\PhysicalDrive0\n"
      "diskimage /write file.img \\\\.\\A:\n"
      "To clone input drive 0 to output drive 1:\n"
      "diskimage /clone \\\\.\\PhysicalDrive0 \\\\.\\PhysicalDrive1\n"
      );
    return 1;
  }
  if (_wcsicmp(argv[1], L"/save") == 0)
  {
    return save(argv[2], argv[3]);
  }
  else if (_wcsicmp(argv[1], L"/write") == 0)
  {
    return write(argv[2], argv[3]);
  }
  else if (_wcsicmp(argv[1], L"/clone") == 0)
  {
    return clone(argv[2], argv[3]);
  }
  else
  {
    printf("Invalid argument.  Use /? for syntax help.\n");
    return 1;
  }
}