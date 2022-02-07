#pragma once

#include <Windows.h>
#include <stdint.h>

/*external funcs*/
uint32_t OpenDeviceInternalA(_In_ char *deviceName, _In_ BOOL fatalOnError,
                             _In_ BOOL asyncMode, _Out_ HANDLE *deviceHandle);

uint32_t GetAssociatedPoolW(_In_ wchar_t *poolName, _Out_ GUID *outPoolGUID);
uint32_t GetAssociatedPoolA(_In_ char *poolName, _Out_ GUID *outPoolGUID);

uint32_t CheckCreateTier(_In_ GUID poolGUID);
uint32_t GetLeak(_In_ GUID poolGUID);